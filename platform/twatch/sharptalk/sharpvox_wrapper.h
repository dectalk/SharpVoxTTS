#ifndef SHARPVOX_WRAPPER_H
#define SHARPVOX_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sharpvox_buffer_cb)(const int16_t* buf, int32_t len, int phoneme_id, void* userdata);

void sharpvox_init(void);
void sharpvox_speak(const char* text, sharpvox_buffer_cb on_buffer, void* userdata);
void sharpvox_terminate(void);

#ifdef __cplusplus
}
#endif

#endif // SHARPVOX_WRAPPER_H
