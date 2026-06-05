#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2S_BCLK_GPIO   26
#define I2S_LRCLK_GPIO  25
#define I2S_DOUT_GPIO   33

#define I2S_SAMPLE_RATE   22050
#define I2S_BITS          16       /* bits per sample */

#define I2S_MAX_VOLUME    10       /* unity gain level */

esp_err_t i2s_audio_init(void);
esp_err_t i2s_audio_deinit(void);
void i2s_audio_set_volume(uint8_t level);
size_t i2s_audio_write(const int16_t *samples, size_t count);
void i2s_audio_play_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume);
void i2s_audio_play_scale(void);
void i2s_audio_play_chime(void);
void i2s_audio_speak(const char *text);
void i2s_audio_stop_speech(void);
typedef void (*i2s_speech_cb_t)(const int16_t *samples, int count, int phoneme_id, void *userdata);
void i2s_audio_set_speech_cb(i2s_speech_cb_t cb, void *userdata);

#ifdef __cplusplus
}
#endif
