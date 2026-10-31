#ifndef SHARPVOX_DTCOMPAT_H
#define SHARPVOX_DTCOMPAT_H

#ifdef WAVE_FORMAT_1M16
#undef WAVE_FORMAT_1M16
#endif
#ifdef WAVE_FORMAT_08M16
#undef WAVE_FORMAT_08M16
#endif

#define WAVE_FORMAT_1M16 0x0001
#define WAVE_FORMAT_08M16 0x0002

#define ERR_NOERROR 0
#define ERR_ERROR 1
#define ERR_RESET 2
#define ERR_INDEX 3

#ifndef DT_EXTERN
#if defined(BLD_DECTALK_DLL) && defined(_WIN32)
#define DT_EXTERN extern __declspec(dllexport)
#elif defined(_WIN32)
#define DT_EXTERN extern __declspec(dllimport)
#else
#define DT_EXTERN extern
#endif
#endif

#ifdef __cplusplus
#include "SharpVox.h"

typedef class DTHandle* DT_HANDLE;

class DTHandle {
public:
	SharpVox::SharpVoxSpeaker speaker;

	short* (*callback)(short*, long, int);
	short* (*callback_ex)(DT_HANDLE, short*, long, int);
};
#else
typedef void* DT_HANDLE;
#endif

#ifdef __cplusplus
extern "C" {
#endif

DT_EXTERN void TextToSpeechSafeInit(void);

DT_EXTERN DT_HANDLE TextToSpeechAllocate(void);
DT_EXTERN void TextToSpeechFree(DT_HANDLE tts);
DT_EXTERN int TextToSpeechStartEx(DT_HANDLE tts, char* input, short* buffer_deprecated, int output_format);
DT_EXTERN int TextToSpeechInitEx(DT_HANDLE tts, short* (*callback)(short*, long, int), short* (*callback_ex)(DT_HANDLE, short*, long, int), void* user_dict);
DT_EXTERN int TextToSpeechResetEx(DT_HANDLE tts);
DT_EXTERN int TextToSpeechSyncEx(DT_HANDLE tts);
DT_EXTERN int TextToSpeechChangeVoiceEx(DT_HANDLE tts, const char* cvoice);
DT_EXTERN void TextToSpeechSetRateEx(DT_HANDLE tts, int rate);
DT_EXTERN int TextToSpeechSetVoiceParamEx(DT_HANDLE tts, const char* cmd, int value);
DT_EXTERN short TextToSpeechGetSpdefValueEx(DT_HANDLE tts, int index);

DT_EXTERN int TextToSpeechStart(char* input, short* buffer_deprecated, int output_format);
DT_EXTERN int TextToSpeechInit(short* (*callback)(short*, long, int), void* user_dict);
DT_EXTERN int TextToSpeechReset(void);
DT_EXTERN int TextToSpeechSync(void);
DT_EXTERN int TextToSpeechChangeVoice(const char* cvoice);
DT_EXTERN void TextToSpeechSetRate(int rate);
DT_EXTERN int TextToSpeechSetVoiceParam(const char* cmd, int value);
DT_EXTERN short TextToSpeechGetSpdefValue(int index);

#ifdef __cplusplus
}
#endif

#endif // SHARPVOX_DTCOMPAT_H
