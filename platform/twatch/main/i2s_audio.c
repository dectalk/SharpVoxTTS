#include "i2s_audio.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <string.h>
#include <stdlib.h>

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "i2s_audio";
static i2s_chan_handle_t s_tx_handle = NULL;
static uint8_t           s_volume    = I2S_MAX_VOLUME;

#define SINE_TABLE_SIZE 256
static int16_t s_sine_table[SINE_TABLE_SIZE];
static bool    s_sine_ready = false;

static void build_sine_table(void) {
    if (s_sine_ready) return;
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        s_sine_table[i] = (int16_t)(32767.0f * sinf(2.0f * M_PI * i / SINE_TABLE_SIZE));
    }
    s_sine_ready = true;
}

static inline int16_t apply_volume(int16_t sample, uint8_t vol) {
    if (vol == 0) return 0;
    if (vol >= I2S_MAX_VOLUME) return sample;
    return (int16_t)((int32_t)sample * vol / I2S_MAX_VOLUME);
}
// API stuffs
esp_err_t i2s_audio_init(void) {
    build_sine_table();

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    chan_cfg.dma_desc_num  = 6;
    chan_cfg.dma_frame_num = 256;

    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx_handle, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_LRCLK_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;

    err = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
        return err;
    }

    err = i2s_channel_enable(s_tx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
        return err;
    }

    ESP_LOGI(TAG, "I2S TX online: %d Hz, 16-bit stereo → MAX98357A",
             I2S_SAMPLE_RATE);
    ESP_LOGI(TAG, "  BCLK=GPIO%d  LRCLK=GPIO%d  DOUT=GPIO%d",
             I2S_BCLK_GPIO, I2S_LRCLK_GPIO, I2S_DOUT_GPIO);

    return ESP_OK;
}

esp_err_t i2s_audio_deinit(void) {
    if (!s_tx_handle) return ESP_OK;
    i2s_channel_disable(s_tx_handle);
    esp_err_t err = i2s_del_channel(s_tx_handle);
    s_tx_handle = NULL;
    return err;
}

void i2s_audio_set_volume(uint8_t level) {
    if (level > I2S_MAX_VOLUME) level = I2S_MAX_VOLUME;
    s_volume = level;
    ESP_LOGD(TAG, "Volume set to %d/%d", s_volume, I2S_MAX_VOLUME);
}

size_t i2s_audio_write(const int16_t *samples, size_t count) {
    if (!s_tx_handle || !samples || count == 0) return 0;

    if (s_volume >= I2S_MAX_VOLUME) {
        size_t bytes_written = 0;
        i2s_channel_write(s_tx_handle, samples,
                          count * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        return bytes_written / sizeof(int16_t);
    }

    static int16_t vol_buf[512];   /* static to avoid stack overflow */
    size_t written = 0;
    while (written < count) {
        size_t chunk = count - written;
        if (chunk > 512) chunk = 512;
        for (size_t i = 0; i < chunk; i++) {
            vol_buf[i] = apply_volume(samples[written + i], s_volume);
        }
        size_t bytes_written = 0;
        i2s_channel_write(s_tx_handle, vol_buf,
                          chunk * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        written += bytes_written / sizeof(int16_t);
    }
    return written;
}

void i2s_audio_play_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume) {
    if (!s_tx_handle) return;
    if (freq_hz == 0 || duration_ms == 0) return;

    uint8_t prev_vol = s_volume;
    i2s_audio_set_volume(volume);

    const uint32_t SCALE = 256;
    uint64_t total_frames   = (uint64_t)I2S_SAMPLE_RATE * duration_ms / 1000;
    uint64_t phase_inc_fp   = (uint64_t)SINE_TABLE_SIZE * freq_hz * SCALE / I2S_SAMPLE_RATE;
    uint64_t phase_fp       = 0;

    static int16_t buf[512];   /* 256 stereo frames */
    uint64_t frames_done = 0;

    while (frames_done < total_frames) {
        uint32_t frames_now = 256;
        if (frames_done + frames_now > total_frames) {
            frames_now = (uint32_t)(total_frames - frames_done);
        }

        for (uint32_t f = 0; f < frames_now; f++) {
            uint32_t idx = (phase_fp / SCALE) % SINE_TABLE_SIZE;
            int16_t s = apply_volume(s_sine_table[idx], s_volume);
            buf[f * 2 + 0] = s;   /* left  */
            buf[f * 2 + 1] = s;   /* right */
            phase_fp += phase_inc_fp;
        }

        size_t bytes_written = 0;
        i2s_channel_write(s_tx_handle, buf,
                          frames_now * 2 * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        frames_done += frames_now;
    }

    memset(buf, 0, sizeof(buf));
    for (int i = 0; i < 8; i++) {
        size_t bytes_written = 0;
        i2s_channel_write(s_tx_handle, buf, sizeof(buf), &bytes_written, portMAX_DELAY);
    }

    i2s_audio_set_volume(prev_vol);
}

void i2s_audio_play_scale(void) {
    static const uint32_t notes[] = { 262, 294, 330, 349, 392 };
    static const uint32_t dur_ms  = 180;

    for (int i = 0; i < 5; i++) {
        i2s_audio_play_tone(notes[i], dur_ms, 6);
        vTaskDelay(pdMS_TO_TICKS(20));  /* brief gap between notes */
    }
}

void i2s_audio_play_chime(void) {
    i2s_audio_play_tone(880, 120, 5);   /* A5 */
    vTaskDelay(pdMS_TO_TICKS(30));
    i2s_audio_play_tone(660, 200, 5);   /* E5 */
    vTaskDelay(pdMS_TO_TICKS(20));
}

// SharpVox integration things
#include "sharpvox_wrapper.h"

static i2s_speech_cb_t s_speech_cb       = NULL;
static void*           s_speech_userdata = NULL;
static volatile bool   s_stop_requested  = false;
static bool            s_sharpvox_inited = false;

void i2s_audio_set_speech_cb(i2s_speech_cb_t cb, void *userdata) {
    s_speech_cb       = cb;
    s_speech_userdata = userdata;
}

void i2s_audio_stop_speech(void) {
    s_stop_requested = true;
}

static void speech_stream_cb(const int16_t *samples, int32_t count, int phoneme_id, void *userdata) {
    if (s_stop_requested) return;

    if (s_speech_cb) {
        s_speech_cb(samples, count, phoneme_id, s_speech_userdata);
    }

    // SharpVox produces mono samples at I2S_SAMPLE_RATE.
    // Our I2S is stereo  duplicate mono to L=R.

    static int16_t stereo_buf[1024]; /* 512 frames */
    int processed = 0;

    while (processed < count && !s_stop_requested) {
        int chunk = count - processed;
        if (chunk > 512) chunk = 512;

        for (int i = 0; i < chunk; i++) {
            int16_t s = apply_volume(samples[processed + i], s_volume);
            stereo_buf[i * 2 + 0] = s;
            stereo_buf[i * 2 + 1] = s;
        }

        size_t bytes_written = 0;
        i2s_channel_write(s_tx_handle, stereo_buf,
                          chunk * 2 * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        processed += chunk;
    }
}

void i2s_audio_speak(const char *text) {
    if (!s_tx_handle || !text) return;

    s_stop_requested = false;

    if (!s_sharpvox_inited) {
        ESP_LOGI(TAG, "Initialising SharpVox TTS engine…");
        sharpvox_init();
        s_sharpvox_inited = true;
    }

    ESP_LOGI(TAG, "Speaking: \"%s\"", text);
    sharpvox_speak(text, speech_stream_cb, NULL);

    /* Flush silence to ensure the last words are heard */
    int16_t silence[256 * 2] = {0};
    size_t bw;
    for (int i = 0; i < 4; i++) {
        i2s_channel_write(s_tx_handle, silence, sizeof(silence), &bw, portMAX_DELAY);
    }
}
