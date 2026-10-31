#include "DTCompat.h"
#include "../../include/VoicePresets.h"

#include <cstring>

static DT_HANDLE global = nullptr;

void TextToSpeechSafeInit(void){
	/* dummy */
}

DT_HANDLE TextToSpeechAllocate(void){
	DT_HANDLE tts = new DTHandle();

	return tts;
}

void TextToSpeechFree(DT_HANDLE tts){
	delete tts;
}

/* TODO: phoneme */
static void onBuffer(SharpVox::SharpVoxSpeaker* speaker, const int16_t* buf, int32_t len, void* userdata){
	DT_HANDLE tts = (DT_HANDLE)userdata;

	if(tts->callback != nullptr) tts->callback((short*)buf, len, 0);
	if(tts->callback_ex != nullptr) tts->callback_ex(tts, (short*)buf, len, 0);
}

int TextToSpeechStartEx(DT_HANDLE tts, char* input, short* buffer_deprecated, int output_format){
	tts->speaker.SampleRate = output_format == WAVE_FORMAT_1M16 ? 11025 : (output_format == WAVE_FORMAT_08M16 ? 8000 : 22050);
	tts->speaker.ApplyVoiceInPlace();

	tts->speaker.Speak(std::string(input), onBuffer, tts);

	return ERR_NOERROR;
}

int TextToSpeechInitEx(DT_HANDLE tts, short* (*callback)(short*, long, int), short* (*callback_ex)(DT_HANDLE, short*, long, int), void* user_dict){
	tts->callback = callback;
	tts->callback_ex = callback_ex;

	return ERR_NOERROR;
}

int TextToSpeechResetEx(DT_HANDLE tts){
	/* TODO */
	tts->speaker.ApplyVoiceData(SharpVox::VoiceData::baseline_voice());
	return ERR_NOERROR;
}

int TextToSpeechSyncEx(DT_HANDLE tts){
	/* dummy */
	return ERR_NOERROR;
}

int TextToSpeechChangeVoiceEx(DT_HANDLE tts, const char* cvoice){
	SharpVox::VoiceData voiceData;

	/* TODO: make mapping */
	return ERR_ERROR;

	if(SharpVox::VoicePresets::TryGet(cvoice, voiceData)){
		tts->speaker.ApplyVoiceData(voiceData);
	}
	return ERR_NOERROR;
}

void TextToSpeechSetRateEx(DT_HANDLE tts, int rate){
	tts->speaker.Rate = rate;
	tts->speaker.ApplyVoiceInPlace();
}

int TextToSpeechSetVoiceParamEx(DT_HANDLE tts, const char* cmd, int value){
	/* dummy */
	return ERR_ERROR;
}

short TextToSpeechGetSpdefValueEx(DT_HANDLE tts, int index){
	/* dummy */
	return 0;
}

int TextToSpeechStart(char* input, short* buffer_deprecated, int output_format){
	return TextToSpeechStartEx(global, input, buffer_deprecated, output_format);
}

int TextToSpeechInit(short* (*callback)(short*, long, int), void* user_dict){
	if(global == nullptr) global = TextToSpeechAllocate();

	return TextToSpeechInitEx(global, callback, nullptr, user_dict);
}

int TextToSpeechReset(void){
	return TextToSpeechResetEx(global);
}

int TextToSpeechSync(void){
	return TextToSpeechSyncEx(global);
}

int TextToSpeechChangeVoice(const char* cvoice){
	return TextToSpeechChangeVoiceEx(global, cvoice);
}

void TextToSpeechSetRate(int rate){
	TextToSpeechSetRateEx(global, rate);
}

int TextToSpeechSetVoiceParam(const char* cmd, int value){
	return TextToSpeechSetVoiceParamEx(global, cmd, value);
}

short TextToSpeechGetSpdefValue(int index){
	return TextToSpeechGetSpdefValueEx(global, index);
}
