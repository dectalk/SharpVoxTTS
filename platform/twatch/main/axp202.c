#include "axp202.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "axp202";

// I2C helpers
esp_err_t axp202_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    esp_err_t err = i2c_master_write_to_device(
        AXP202_I2C_PORT,
        AXP202_I2C_ADDR,
        buf, sizeof(buf),
        pdMS_TO_TICKS(100)
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "write reg 0x%02X = 0x%02X failed: %s",
                 reg, val, esp_err_to_name(err));
    }
    return err;
}

esp_err_t axp202_read_reg(uint8_t reg, uint8_t *val) {
    esp_err_t err = i2c_master_write_read_device(
        AXP202_I2C_PORT,
        AXP202_I2C_ADDR,
        &reg, 1,
        val, 1,
        pdMS_TO_TICKS(100)
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read reg 0x%02X failed: %s", reg, esp_err_to_name(err));
    }
    return err;
}

// API stuff
esp_err_t axp202_init(void) {
    esp_err_t err;

    // setup I2C driver.  We own the bus in init
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = AXP202_I2C_SDA,
        .scl_io_num       = AXP202_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = AXP202_I2C_FREQ,
    };

    err = i2c_param_config(AXP202_I2C_PORT, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(AXP202_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C master installed on port %d (SDA=%d SCL=%d)",
             AXP202_I2C_PORT, AXP202_I2C_SDA, AXP202_I2C_SCL);

    // LDO2/4 backlight/audio
    uint8_t ldo24_val = AXP202_LDO2_3V3 | AXP202_LDO4_3V3;
    err = axp202_write_reg(AXP202_REG_LDO24_VOLTAGE, ldo24_val);
    if (err != ESP_OK) return err;
    ESP_LOGI(TAG, "LDO2/4 voltages set to 3.3 V (reg 0x28 = 0x%02X)", ldo24_val);

    // LDO3 is the mic
    err = axp202_write_reg(AXP202_REG_LDO3_VOLTAGE, AXP202_LDO3_3V3);
    if (err != ESP_OK) return err;
    ESP_LOGI(TAG, "LDO3 voltage set to 3.3 V");

    uint8_t pwr_ctl;
    err = axp202_read_reg(AXP202_REG_LDO234_DC23_CTL, &pwr_ctl);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Power-CTL reg 0x12 before: 0x%02X", pwr_ctl);

    pwr_ctl |= AXP202_LDO2_EN_BIT | AXP202_LDO3_EN_BIT;
    pwr_ctl &= ~AXP202_LDO4_EN_BIT;

    err = axp202_write_reg(AXP202_REG_LDO234_DC23_CTL, pwr_ctl);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Power-CTL reg 0x12 after:  0x%02X  (LDO2+LDO3 enabled, LDO4 pending)", pwr_ctl);

    err = axp202_write_reg(AXP202_REG_ADC_EN1, 0xFF);
    if (err != ESP_OK) return err;

    // short delay to allow rails to settle before the display runs
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "AXP202 init complete");
    return ESP_OK;
}

esp_err_t axp202_set_backlight(bool on) {
    uint8_t pwr_ctl;
    esp_err_t err = axp202_read_reg(AXP202_REG_LDO234_DC23_CTL, &pwr_ctl);
    if (err != ESP_OK) return err;

    if (on) {
        pwr_ctl |=  AXP202_LDO2_EN_BIT;
    } else {
        pwr_ctl &= ~AXP202_LDO2_EN_BIT;
    }

    return axp202_write_reg(AXP202_REG_LDO234_DC23_CTL, pwr_ctl);
}

esp_err_t axp202_set_brightness(uint8_t level) {
    if (level == 0) {
        return axp202_set_backlight(false);
    }

    if (level > AXP202_BL_LEVELS) level = AXP202_BL_LEVELS;

    // Make sure LDO2 is enabled
    esp_err_t err = axp202_set_backlight(true);
    if (err != ESP_OK) return err;

    // map 1-10 to nibble
    uint8_t nibble = 7 + level;   /* level=18, level=1017  clamp to 0xF */
    if (nibble > 0xF) nibble = 0xF;

    uint8_t ldo24_val;
    err = axp202_read_reg(AXP202_REG_LDO24_VOLTAGE, &ldo24_val);
    if (err != ESP_OK) return err;

    ldo24_val = (ldo24_val & 0xF0) | nibble;
    ESP_LOGD(TAG, "Brightness level %d → nibble 0x%X → reg 0x28 = 0x%02X",
             level, nibble, ldo24_val);

    return axp202_write_reg(AXP202_REG_LDO24_VOLTAGE, ldo24_val);
}

esp_err_t axp202_set_speaker_power(bool on) {
    uint8_t pwr_ctl;
    esp_err_t err = axp202_read_reg(AXP202_REG_LDO234_DC23_CTL, &pwr_ctl);
    if (err != ESP_OK) return err;

    if (on) {
        pwr_ctl |=  AXP202_LDO4_EN_BIT;
    } else {
        pwr_ctl &= ~AXP202_LDO4_EN_BIT;
    }

    ESP_LOGI(TAG, "Speaker power %s (reg 0x12 = 0x%02X)", on ? "ON" : "OFF", pwr_ctl);
    return axp202_write_reg(AXP202_REG_LDO234_DC23_CTL, pwr_ctl);
}
