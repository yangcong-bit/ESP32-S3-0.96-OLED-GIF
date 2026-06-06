#ifndef U8G2_ESP32_HAL_H
#define U8G2_ESP32_HAL_H

#include "u8g2.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief U8g2 I2C 字节回调函数
 * @param u8x8  U8x8 设备指针
 * @param msg   消息类型（U8X8_MSG_BYTE_*）
 * @param arg_int  参数整数
 * @param ptr   数据指针
 * @return 0 成功，非0失败
 */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/**
 * @brief U8g2 GPIO 与延时回调函数
 * @param u8x8  U8x8 设备指针
 * @param msg   消息类型（U8X8_MSG_GPIO/Delay*）
 * @param arg_int  参数整数
 * @param ptr   数据指针
 * @return 0 成功，非0失败
 */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#ifdef __cplusplus
}
#endif

#endif // U8G2_ESP32_HAL_H
