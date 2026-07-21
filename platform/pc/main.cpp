#include <Mw/BaseTypes.h>
#include <Mw/Constants.h>
#include <Mw/LowLevel.h>
#include <Mw/Milsko.h>
#include <Mw/StringDefs.h>
#include <Mw/TypeDefs.h>
#include <Mw/Widget/Box.h>
#include <Mw/Widget/Button.h>
#include <Mw/Widget/Entry.h>
#include <Mw/Widget/Frame.h>
#include <Mw/Widget/Image.h>
#include <Mw/Widget/Label.h>
#include <Mw/Widget/ScrollBar.h>
#include <Mw/Widget/Separator.h>

// image assets
#include "pau203a.xpm"
#include "bet203a.xpm"
#include "har203a.xpm"
#include "fra203a.xpm"
#include "den203a.xpm"
#include "kid203a.xpm"
#include "urs203a.xpm"
#include "rit203a.xpm"
#include "wen203a.xpm"

#include "SharpVox.h"
#include "miniaudio.h"
#include <cstring>
#include <stb_ds.h>

MwWidget window, testwindow, mainBox, textInput;

using namespace::SharpVox;

SharpVoxSpeaker speaker;

char *currentVoice = "jack";

void MWAPI resize(MwWidget handle, void* user, void* client) {
        int ww = MwGetInteger(handle, MwNwidth);
        int wh = MwGetInteger(handle, MwNheight);

        (void)user;
        (void)client;

        MwVaApply(mainBox,
                  MwNwidth, ww,
                  MwNheight, wh,
                  NULL);

}

ma_device_config config;
ma_device device;
ma_mutex speaking;

typedef struct buffer {
	short* data;
	unsigned int length;
	unsigned int seek;
} buffer_t;

buffer_t* buffers = NULL;

unsigned int g_length;
short* g_wave = NULL;

void data_callback(ma_device* dev, void* out, const void* in, ma_uint32 frame){
	unsigned int seek = 0;
	memset(out, 0, frame * 2);
	ma_mutex_lock(&speaking);
	while(frame > 0 && arrlen(buffers) > 0){
		unsigned int bsz = buffers[0].length - buffers[0].seek;
		unsigned int sz = bsz > frame ? frame : bsz;

		memcpy(((short*)out) + seek, buffers[0].data + buffers[0].seek, sz * 2);

		buffers[0].seek += sz;
		frame -= sz;
		seek += sz;

		if((buffers[0].length - buffers[0].seek) == 0){
			free(buffers[0].data);
			arrdel(buffers, 0);
		}
	}
	ma_mutex_unlock(&speaking);
}

void forceStop() {
    // force it to stop
    ma_mutex_lock(&speaking);
	if(arrlen(buffers) > 0){
		while(arrlen(buffers) > 0){
			free(buffers[0].data);
			arrdel(buffers, 0);
		}
	}
	ma_mutex_unlock(&speaking);
}

void onStop(MwWidget handle, void* client, void* user){
    // on stop, stop.
    forceStop();
}

static void speak(SharpVoxSpeaker& speaker, std::string text) {
    // stop previous audio so we can start immediately
    forceStop();

	g_length = 0;

	// set sample rate of tts to match miniaudio
	speaker.SampleRate = 44100;
	speaker.ApplyVoiceInPlace();

	// build input string with voice option
	std::string bufftext = "[:voice ";
	bufftext += currentVoice;
	bufftext += "] ";
	bufftext += text;

    // actually do the speaking
    speaker.Speak(bufftext.c_str(), [](SharpVoxSpeaker* /*speaker*/, const short* buf, int len, void* ud) {
        if(g_wave == NULL){
    		g_wave = (short *) malloc(len * 2);
    		memcpy(g_wave, buf, len * 2);
    	}else{
    		short* old = g_wave;
    		g_wave = (short *) malloc((g_length + len) * 2);
    		memcpy(g_wave, old, g_length * 2);
    		memcpy(g_wave + g_length, buf, len * 2);
    		free(old);
    	}

    	g_length += len;
    });

    buffer_t buf;
    buf.data = (short *) malloc(g_length * 2);
	memcpy(buf.data, g_wave, g_length * 2);
	buf.length = g_length;
	buf.seek = 0;

	if(g_wave != NULL){
	    free(g_wave);
		g_wave = NULL;
	}

	ma_mutex_lock(&speaking);
    arrput(buffers, buf);
    ma_mutex_unlock(&speaking);
}

static void MWAPI onSpeak(MwWidget handle, void* client, void* user) {
    // on speak, speak.
    speak(speaker, MwGetText(textInput, MwNtext));
}

MwWidget makeVoiceButton(MwWidget parent, MwLLPixmap img, std::string voice) {
    MwWidget buttonContainer = MwVaCreateWidget(MwBoxClass, "btn2", parent, 0, 0, 0, 0,
                    MwNorientation, MwHORIZONTAL,
                    NULL);
    MwVaCreateWidget(MwFrameClass, "btn2", buttonContainer, 0, 0, 0, 0,
                    NULL);
    MwWidget button = MwVaCreateWidget(MwButtonClass, voice.c_str(), buttonContainer, 0, 0, 0, 0,
                    MwNpixmap, img,
                    MwNfixedSize, 60,
                    MwNtext, voice.c_str(),
                    NULL);
    MwVaCreateWidget(MwFrameClass, "btn2", buttonContainer, 0, 0, 0, 0,
                    NULL);

    MwAddUserHandler(button, MwNactivateHandler, [](MwWidget handle, void* client, void* user) {
        currentVoice = (char *) MwGetText(handle, MwNtext);
        speak(speaker, currentVoice);
    }, NULL);

    return button;
}

void makeVoiceBar(MwWidget parent) {
    // just associates the buttons with what voice they actually represent
    makeVoiceButton(parent, MwLoadXPM (window, pau203a), "john");
    makeVoiceButton(parent, MwLoadXPM (window, bet203a), "beth");
    makeVoiceButton(parent, MwLoadXPM (window, har203a), "matt");
    makeVoiceButton(parent, MwLoadXPM (window, fra203a), "jack");
    makeVoiceButton(parent, MwLoadXPM (window, den203a), "chris");
    makeVoiceButton(parent, MwLoadXPM (window, kid203a), "tommy");
    makeVoiceButton(parent, MwLoadXPM (window, urs203a), "jess");
    makeVoiceButton(parent, MwLoadXPM (window, rit203a), "deborah");
    makeVoiceButton(parent, MwLoadXPM (window, wen203a), "whisper");
}

int main() {
    // audio init
    config = ma_device_config_init(ma_device_type_playback);
	config.playback.format = ma_format_s16;
	config.playback.channels = 1;
	config.sampleRate = 44100;
	config.dataCallback = data_callback;

	if(ma_device_init(NULL, &config, &device) != MA_SUCCESS || ma_device_start(&device) != MA_SUCCESS){
	    fprintf(stderr, "your sound is broken\n");
		return 1;
	}
	ma_mutex_init(&speaking);

	// gui init
	MwWidget verticalBox, voiceBar, textAreaBox, buttonBar, speakButton, stopButton;

        MwLibraryInit();

        // the main window
        window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 640, 480,
                                  MwNtitle, "SharpVox GUI",
                                  NULL);

        // test window, placeholder for voice config window in the future
        // testwindow = MwVaCreateWidget(MwWindowClass, "main 1", NULL, MwDEFAULT, MwDEFAULT, 600, 200,
        //                           MwNtitle, "SharpVox not so GUI",
        //                           NULL);
        // MwReparent(testwindow,window);

        // primary container
        mainBox = MwVaCreateWidget(MwBoxClass, "box", window, 0, 0, 0, 0,
                               MwNpadding, 10,
                               MwNmargin, 10,
                               NULL);

        // holds top middle and bottom section
        verticalBox = MwVaCreateWidget(MwBoxClass, "box2", mainBox, 0, 0, 0, 0,
                                MwNmargin, 10,
                                MwNorientation, MwVERTICAL,
                                NULL);

        // voice selection bar
        voiceBar = MwVaCreateWidget(MwBoxClass, "btn1", verticalBox, 0, 0, 0, 0,
                         MwNratio, 4,
                         MwNfixedSize, 60,
                         MwNorientation, MwHORIZONTAL,
                         NULL);

        makeVoiceBar(voiceBar);


        // text input area
        textAreaBox = MwVaCreateWidget(MwBoxClass, "btn2", verticalBox, 0, 0, 0, 0,
                         MwNratio, 8,
                         NULL);

        textInput = MwVaCreateWidget(MwEntryClass, "btn2", textAreaBox, 0, 0, 0, 0,
                        MwNtext, "Placeholder text",
                        NULL);

        // bottom button bar
        buttonBar = MwVaCreateWidget(MwBoxClass, "btn3", verticalBox, 0, 0, 0, 0,
                         MwNratio, 1,
                         MwNfixedSize, 30,
                         MwNorientation, MwHORIZONTAL,
                         NULL);

        MwVaCreateWidget(MwLabelClass, "rateLabel", buttonBar, 0, 0, 0, 0,
                         MwNfixedSize, 100,
                         MwNtext, "Speaking Rate",
                         NULL);

        MwVaCreateWidget(MwScrollBarClass, "rateBar", buttonBar, 0, 0, 0, 0,
                         MwNfixedSize, 200,
                         MwNorientation, MwHORIZONTAL,
                         MwNshowArrows, 0,
                         MwNareaShown, 10,
                         NULL);

        MwVaCreateWidget(MwLabelClass, "wpmLabel", buttonBar, 0, 0, 0, 0,
                         MwNorientation, MwHORIZONTAL,
                         MwNtext, "200 WPM",
                         NULL);

        MwVaCreateWidget(MwFrameClass, "separator", buttonBar, 0, 0, 0, 0,
                         MwNorientation, MwHORIZONTAL,
                         NULL);

        stopButton = MwVaCreateWidget(MwButtonClass, "btn2", buttonBar, 0, 0, 0, 0,
                         MwNfixedSize, 64,
                         MwNtext, "stop",
                         NULL);

        speakButton = MwVaCreateWidget(MwButtonClass, "btn2", buttonBar, 0, 0, 0, 0,
                         MwNfixedSize, 64,
                         MwNtext, "play",
                         NULL);

        // handle speak and stop
        MwAddUserHandler(speakButton, MwNactivateHandler, onSpeak, NULL);
        MwAddUserHandler(stopButton, MwNactivateHandler, onStop, NULL);

        // even though window doesnt resize, these are mandatory for initial sizing
        MwAddUserHandler(window, MwNresizeHandler, resize, NULL);
        resize(window, NULL, NULL);


        // window range
        MwSizeHints hints;
        hints.min_width = 640;
        hints.max_width = 640;
        hints.min_height = 480;
        hints.max_height = 480;
        MwVaApply(window, MwNsizeHints, &hints, NULL);

        // loop it
        MwLoop(window);
}
