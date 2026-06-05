#include "st7789.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "st7789";
static esp_lcd_panel_io_handle_t  s_io_handle  = NULL;
static esp_lcd_panel_handle_t     s_panel      = NULL;

#define STRIP_ROWS  16
static uint16_t s_strip[STRIP_ROWS * LCD_WIDTH];

#define SPI_MAX_TRANSFER_BYTES  (STRIP_ROWS * LCD_WIDTH * sizeof(uint16_t) + 64)

esp_err_t st7789_init(void) {
    esp_err_t err;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LCD_MOSI_GPIO,
        .miso_io_num     = -1,           /* display is write-only */
        .sclk_io_num     = LCD_SCLK_GPIO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = SPI_MAX_TRANSFER_BYTES,
        .flags           = SPICOMMON_BUSFLAG_MASTER,
    };

    err = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "SPI2 bus initialised (MOSI=%d SCLK=%d)",
             LCD_MOSI_GPIO, LCD_SCLK_GPIO);

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num         = LCD_DC_GPIO,
        .cs_gpio_num         = LCD_CS_GPIO,
        .pclk_hz             = LCD_SPI_CLOCK,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .spi_mode            = 3,   /* CPOL=1 CPHA=1  reliable on T-Watch */
        .trans_queue_depth   = 1,   /* sync  s_strip is reused, async would corrupt */
        .on_color_trans_done = NULL,
        .user_ctx            = NULL,
        .flags = {
            .dc_low_on_data = 0,    /* DC low = command, DC high = data */
        },
    };

    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,
                                   &io_cfg, &s_io_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num  = -1,
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian     = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel  = 16,
    };

    err = esp_lcd_new_panel_st7789(s_io_handle, &panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_st7789 failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Running ST7789V init sequence…");
    err = esp_lcd_panel_init(s_panel);
    if (err != ESP_OK) return err;

    err = esp_lcd_panel_invert_color(s_panel, true);
    if (err != ESP_OK) return err;

    err = esp_lcd_panel_swap_xy(s_panel, false);
    if (err != ESP_OK) return err;

    err = esp_lcd_panel_mirror(s_panel, false, true);
    if (err != ESP_OK) return err;

    err = esp_lcd_panel_set_gap(s_panel, 0, 80);
    if (err != ESP_OK) return err;

    err = esp_lcd_panel_disp_on_off(s_panel, true);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "ST7789V online — %d×%d, RGB565", LCD_WIDTH, LCD_HEIGHT);

    st7789_fill(COLOR_BLACK);
    return ESP_OK;
}

static inline uint16_t swap16(uint16_t v) {
    return (v >> 8) | (v << 8);
}

void st7789_blit_rgb565(int x0, int y0, int x1, int y1, const uint16_t *data) {
    if (x0 < 0)           x0 = 0;
    if (y0 < 0)           y0 = 0;
    if (x1 >= LCD_WIDTH)  x1 = LCD_WIDTH  - 1;
    if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
    if (x0 > x1 || y0 > y1) return;

    esp_lcd_panel_draw_bitmap(s_panel, x0, y0, x1 + 1, y1 + 1,  (const void *)data);
}

void st7789_blit_rgb565_native(int x0, int y0, int x1, int y1, const uint16_t *data, size_t count) {
    if (x0 < 0)           x0 = 0;
    if (y0 < 0)           y0 = 0;
    if (x1 >= LCD_WIDTH)  x1 = LCD_WIDTH  - 1;
    if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
    if (x0 > x1 || y0 > y1) return;

    int w = x1 - x0 + 1;
    int strip_cap = sizeof(s_strip) / sizeof(s_strip[0]);
    int rows_per_strip = strip_cap / w;
    if (rows_per_strip < 1) rows_per_strip = 1;

    const uint16_t *src = data;
    for (int y = y0; y <= y1; ) {
        int rows_left   = y1 - y + 1;
        int rows_now    = (rows_left < rows_per_strip) ? rows_left : rows_per_strip;
        int pixels_now  = w * rows_now;

        for (int i = 0; i < pixels_now; i++) {
            s_strip[i] = swap16(src[i]);
        }
        esp_lcd_panel_draw_bitmap(s_panel,
                                  x0, y,
                                  x1 + 1, y + rows_now,
                                  s_strip);
        src += pixels_now;
        y   += rows_now;
    }
}

void st7789_fill_rect(int x0, int y0, int x1, int y1, uint16_t color) {
    if (x0 < 0)           x0 = 0;
    if (y0 < 0)           y0 = 0;
    if (x1 >= LCD_WIDTH)  x1 = LCD_WIDTH  - 1;
    if (y1 >= LCD_HEIGHT) y1 = LCD_HEIGHT - 1;
    if (x0 > x1 || y0 > y1) return;

    int w = x1 - x0 + 1;
    uint16_t fill_color = swap16(color);  /* swap once for the whole rect */

    int strip_pixels = w * STRIP_ROWS;
    if (strip_pixels > (int)(sizeof(s_strip) / sizeof(s_strip[0]))) {
        strip_pixels = sizeof(s_strip) / sizeof(s_strip[0]);
    }
    for (int i = 0; i < strip_pixels; i++) {
        s_strip[i] = fill_color;
    }

    for (int y = y0; y <= y1; ) {
        int rows_left    = y1 - y + 1;
        int rows_to_send = (rows_left < STRIP_ROWS) ? rows_left : STRIP_ROWS;

        esp_lcd_panel_draw_bitmap(s_panel,
                                  x0, y,
                                  x1 + 1, y + rows_to_send,
                                  s_strip);
        y += rows_to_send;
    }
    esp_lcd_panel_io_tx_param(s_io_handle, 0x00 /*ST7789 NOP*/, NULL, 0);
}

void st7789_fill(uint16_t color) {
    st7789_fill_rect(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, color);
}

static const uint8_t s_font5x7[27][5] = {
    {0x7E,0x09,0x09,0x09,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x01,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x3A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x09,0x19,0x66}, /* R */
    {0x26,0x49,0x49,0x49,0x32}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x00,0x00,0x00,0x00}, /* space */
};

void st7789_draw_label(int cx, int cy, const char *text, uint16_t fg, uint16_t bg, int scale) {
    if (!text || scale < 1) return;
    int len = 0;
    for (const char *p = text; *p; p++) len++;

    int char_w = 5 * scale;
    int gap     = scale;
    int total_w = len * char_w + (len > 1 ? (len - 1) * gap : 0);
    int total_h = 7 * scale;
    int x0 = cx - total_w / 2;
    int y0 = cy - total_h / 2;

    st7789_fill_rect(x0, y0, x0 + total_w - 1, y0 + total_h - 1, bg);

    int cx_char = x0;
    for (int ci = 0; ci < len; ci++) {
        char c = text[ci];
        int idx = (c >= 'A' && c <= 'Z') ? (c - 'A') : 26;
        const uint8_t *glyph = s_font5x7[idx];
        for (int col = 0; col < 5; col++) {
            uint8_t colbits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (colbits & (1 << row)) {
                    int px = cx_char + col * scale;
                    int py = y0 + row * scale;
                    st7789_fill_rect(px, py,
                                     px + scale - 1, py + scale - 1, fg);
                }
            }
        }
        cx_char += char_w + gap;
    }
}
