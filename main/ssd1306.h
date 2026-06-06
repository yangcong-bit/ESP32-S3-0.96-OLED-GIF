#ifndef SSD1306_H
#define SSD1306_H

#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C 硬件配置
#define SSD1306_I2C_NUM         I2C_NUM_0
#define SSD1306_I2C_SDA_PIN     GPIO_NUM_13
#define SSD1306_I2C_SCL_PIN     GPIO_NUM_12
#define SSD1306_I2C_FREQ_HZ     400000
#define SSD1306_I2C_ADDR        0x3C

// OLED 屏幕参数
#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64
#define SSD1306_PAGE_NUM        8

// 控制字节
#define SSD1306_CONTROL_CMD     0x00
#define SSD1306_CONTROL_DATA    0x40

// 基础驱动函数声明
esp_err_t ssd1306_i2c_init(void);
esp_err_t ssd1306_init(void);
esp_err_t ssd1306_write_command(uint8_t cmd);
esp_err_t ssd1306_write_data(const uint8_t *data, size_t len);
esp_err_t ssd1306_set_cursor(uint8_t page, uint8_t column);
esp_err_t ssd1306_clear(void);
esp_err_t ssd1306_fill(uint8_t pattern);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H
