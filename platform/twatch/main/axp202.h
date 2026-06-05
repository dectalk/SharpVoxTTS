#pragma once

#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXP202_I2C_PORT   I2C_NUM_0
#define AXP202_I2C_SDA    21
#define AXP202_I2C_SCL    22
#define AXP202_I2C_FREQ   400000   /* 400 kHz AXP202 supports up to 400 kHz */
#define AXP202_I2C_ADDR   0x35     /* 7-bit address; R/W bit appended by driver */

#define AXP202_REG_LDO234_DC23_CTL  0x12
#define AXP202_REG_LDO24_VOLTAGE    0x28
#define AXP202_REG_LDO3_VOLTAGE     0x29
#define AXP202_REG_CHARGE1          0x33
#define AXP202_REG_ADC_EN1          0x82

#define AXP202_LDO2_EN_BIT   (1 << 2)   /* backlight rail          */
#define AXP202_LDO3_EN_BIT   (1 << 6)   /* LDO3 (unused/mic?)      */
#define AXP202_LDO4_EN_BIT   (1 << 3)   /* audio amplifier rail    */
#define AXP202_DCDC2_EN_BIT  (1 << 0)   /* ESP32 core, never turn off! */

#define AXP202_LDO3_3V3  0x68   /* 3.3 V */
#define AXP202_LDO2_3V3  0x0F   /* lower nibble = 0xF  3.3 V for LDO2 */
#define AXP202_LDO4_3V3  0xF0   /* upper nibble = 0xF  3.3 V for LDO4 */

#define AXP202_BL_LEVELS  10   /* number of brightness steps */

esp_err_t axp202_init(void);
esp_err_t axp202_set_backlight(bool on);
esp_err_t axp202_set_brightness(uint8_t level);
esp_err_t axp202_set_speaker_power(bool on);
esp_err_t axp202_write_reg(uint8_t reg, uint8_t val);
esp_err_t axp202_read_reg(uint8_t reg, uint8_t *val);

#ifdef __cplusplus
}
#endif
