#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_MOSI_GPIO   19
#define LCD_SCLK_GPIO   18
#define LCD_CS_GPIO      5
#define LCD_DC_GPIO     27
// reset pin unused

#define LCD_WIDTH   240
#define LCD_HEIGHT  240

#define LCD_SPI_HOST    SPI2_HOST
#define LCD_SPI_CLOCK   40000000   /* 40 MHz */

#define RGB565(r, g, b) \
    ((uint16_t)(((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_BG_BLUE 0xD71F   /* #D2E3FC (Google Blue 50) */

esp_err_t st7789_init(void);
void st7789_fill(uint16_t color);
void st7789_blit_rgb565(int x0, int y0, int x1, int y1, const uint16_t *data);
void st7789_blit_rgb565_native(int x0, int y0, int x1, int y1, const uint16_t *data, size_t count);
void st7789_draw_label(int cx, int cy, const char *text, uint16_t fg, uint16_t bg, int scale);
void st7789_fill_rect(int x0, int y0, int x1, int y1, uint16_t color);

#ifdef __cplusplus
}
#endif
