#include <jni.h>
#include <string>
#include <cstring>

#include "SharpVox.h"

#define MAX_BUFFER (10 * 60) * 22050
static short samples[MAX_BUFFER];
static int total_size = 0;
static int halting = 0;
static int callback_enabled = 0;

static SharpVox::SharpVoxSpeaker* g_speaker = nullptr;

struct CallbackInfo {
    JavaVM* jvm = nullptr;
    jobject callbackObject = nullptr;
    jclass callbackClass = nullptr;
    jmethodID callbackMethod = nullptr;
};
static CallbackInfo g_callbackInfo;

static void onBuffer(SharpVox::SharpVoxSpeaker* /*speaker*/, const int16_t* buf, int32_t len, void* /*userdata*/) {
    if (halting) return;

    if (total_size + len <= MAX_BUFFER) {
        std::memcpy(samples + total_size, buf, len * sizeof(short));
        total_size += len;
    }

    if (callback_enabled && g_callbackInfo.jvm && g_callbackInfo.callbackObject) {
        JNIEnv* env = nullptr;
        bool attached = false;
        int status = g_callbackInfo.jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            if (g_callbackInfo.jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
            attached = true;
        } else if (status != JNI_OK) {
            return;
        }

        jshortArray jarr = env->NewShortArray(len);
        env->SetShortArrayRegion(jarr, 0, len, buf);
        env->CallVoidMethod(g_callbackInfo.callbackObject, g_callbackInfo.callbackMethod, jarr, (jint)len);

        if (attached) {
            g_callbackInfo.jvm->DetachCurrentThread();
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeInit(JNIEnv* /*env*/, jobject /*obj*/) {
    if (!g_speaker) {
        g_speaker = new SharpVox::SharpVoxSpeaker();
    }
    total_size = 0;
    halting = 0;
    callback_enabled = 0;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeReset(JNIEnv* /*env*/, jobject /*obj*/) {
    total_size = 0;
    halting = 0;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetRate(JNIEnv* /*env*/, jobject /*obj*/, jint rate) {
    if (g_speaker) g_speaker->Rate = rate;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetPitch(JNIEnv* /*env*/, jobject /*obj*/, jint pitch) {
    if (g_speaker) g_speaker->PitchHz = pitch;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetVolume(JNIEnv* /*env*/, jobject /*obj*/, jfloat volume) {
    if (g_speaker) g_speaker->AudioVolume = volume;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSetVoice(JNIEnv* env, jobject /*obj*/, jstring preset) {
    const char* str = env->GetStringUTFChars(preset, nullptr);
    std::string name(str);
    env->ReleaseStringUTFChars(preset, str);

    if (!g_speaker) return;

    if (name == "whisper") {
        g_speaker->SetPreset(SharpVox::VoicePreset::Whisper);
    } else {
        g_speaker->SetPreset(SharpVox::VoicePreset::Baseline);
    }
}

extern "C" JNIEXPORT jshortArray JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSpeak(JNIEnv* env, jobject /*obj*/, jstring text, jboolean enableCallback) {
    if (!g_speaker) return env->NewShortArray(0);

    const char* textStr = env->GetStringUTFChars(text, nullptr);
    std::string text_cpp(textStr);
    env->ReleaseStringUTFChars(text, textStr);

    total_size = 0;
    callback_enabled = enableCallback;

    g_speaker->Speak(text_cpp, onBuffer, nullptr);

    jsize length = total_size;
    jshortArray result = env->NewShortArray(length);
    env->SetShortArrayRegion(result, 0, length, samples);
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_App_nativeSync(JNIEnv* /*env*/, jobject /*obj*/, jint /*what*/) {
    halting = 1;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_bytesizedfox_sharpvoxapp_tts_TtsService_SetCallback(JNIEnv* env, jobject obj) {
    env->GetJavaVM(&g_callbackInfo.jvm);
    jclass localClass = env->GetObjectClass(obj);
    g_callbackInfo.callbackClass = (jclass)env->NewGlobalRef(localClass);
    g_callbackInfo.callbackObject = env->NewGlobalRef(obj);
    g_callbackInfo.callbackMethod = env->GetMethodID(
        g_callbackInfo.callbackClass,
        "javaCallback", "([SI)V");
}
