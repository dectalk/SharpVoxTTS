// main.c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "axp202.h"
#include "st7789.h"
#include "visemes.h"
#include "i2s_audio.h"
#include "ble_spp.h"
#include "sharpvox_wrapper.h"

static const char *TAG = "sharptalk_watch";
static int currentVoice = 0;

// Lipsync state
#define LABEL_CX    120
#define LABEL_CY    120
#define LABEL_SCALE 6

static int s_last_phoneme = -2;

// enough to cover any label at LABEL_SCALE=6 (max 2 chars = 6642 px)
#define LABEL_CLEAR_W  40
#define LABEL_CLEAR_H  26

static void draw_phoneme_label(int phoneme_id) {
    if (phoneme_id == s_last_phoneme) return;
    s_last_phoneme = phoneme_id;

    // Always erase the fixed region so short labels dont leave wide-label ghosting stuff
    st7789_fill_rect(LABEL_CX - LABEL_CLEAR_W, LABEL_CY - LABEL_CLEAR_H,
                     LABEL_CX + LABEL_CLEAR_W, LABEL_CY + LABEL_CLEAR_H, COLOR_BLACK);

    if (phoneme_id >= 0 && phoneme_id < PHONEME_LABEL_COUNT) {
        const char *label = phoneme_to_label[phoneme_id];
        if (label)
            st7789_draw_label(LABEL_CX, LABEL_CY, label, COLOR_WHITE, COLOR_BLACK, LABEL_SCALE);
    }
}

static void lipsync_handler(const int16_t *samples, int count, int phoneme_id, void *userdata) {
    (void)samples; (void)count; (void)userdata;
    draw_phoneme_label(phoneme_id);
}

static void reset_mouth(void) {
    s_last_phoneme = -2;
    draw_phoneme_label(-1);
}

// Talker Task

static QueueHandle_t s_talk_queue = NULL;

static void talker_task(void *pvParameters) {
    char *text;
    while (1) {
        if (xQueueReceive(s_talk_queue, &text, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Talker task speaking: %s", text);
            st7789_fill(COLOR_BLACK);
            axp202_set_brightness(10);
            axp202_set_speaker_power(true);
            vTaskDelay(pdMS_TO_TICKS(50));
            i2s_audio_speak(text);
            reset_mouth();
            free(text);
            // dont power down if there is more to say immediately
            if (uxQueueMessagesWaiting(s_talk_queue) == 0) {
                vTaskDelay(pdMS_TO_TICKS(300));
                if (uxQueueMessagesWaiting(s_talk_queue) == 0) {
                    axp202_set_speaker_power(false);
                    axp202_set_brightness(2);
                }
            }
        }
    }
}

// "you have mail" -AOL
static void on_ble_recv(const char *line) {
    ESP_LOGI(TAG, "BLE received: %s", line);

    // INTERRUPT: signal audio to stop
    i2s_audio_stop_speech();

    // drain the queue to ensure this new message is next
    char *old_text;
    while (xQueueReceive(s_talk_queue, &old_text, 0) == pdTRUE) {
        free(old_text);
    }

    // Work on a copy so we can safely modify and parse it
    char *text = strdup(line);
    if (!text) return;

    if (xQueueSend(s_talk_queue, &text, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Talk queue full, dropping message");
        free(text);
    }
}

// entry point
void app_main(void) {
    // setup power drivers
    axp202_init();
    axp202_set_brightness(2);

    // setup display drivers
    st7789_init();
    st7789_fill(COLOR_BLACK);

    // setup audio drivers
    i2s_audio_init();
    i2s_audio_set_volume(7);
    i2s_audio_set_speech_cb(lipsync_handler, NULL);

    ESP_LOGI(TAG, "initialising SharpVox TTS engine…");
    sharpvox_init();

    // get it ready to speak
    s_talk_queue = xQueueCreate(10, sizeof(char *));
    xTaskCreatePinnedToCore(talker_task, "talker", 32768, NULL, 1, NULL, 1);

    // bluetooth, innit?
    ble_spp_init(on_ble_recv);

    char *startup = strdup("SharpVox is running");
    xQueueSend(s_talk_queue, &startup, 0);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
