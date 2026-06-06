#include "u8g2_esp32_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rom/ets_sys.h"

static const char *TAG = "u8g2_esp32_hal";

// I2C 地址（U8g2 传入的地址是 7-bit 原始地址，需要左移1位变成8-bit地址再 | WRITE）
#define U8G2_I2C_ADDR    0x3C

// I2C 发送缓冲区，暂存 U8g2 逐字节发来的数据
// 全量显存模式下单次传输最多 128+1 字节（命令+数据）
static uint8_t i2c_buffer[129];
static uint8_t i2c_buffer_idx = 0;

/**
 * @brief U8g2 GPIO 与延时回调函数
 *
 * U8g2 渲染过程中会频繁调用此回调来处理延时和 GPIO 操作。
 * 本适配中主要处理延时类消息，GPIO 类消息因使用软件 I2C 而非必须实现。
 */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
        // 毫秒级延时，U8g2 在初始化序列中会调用
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;

        // 10微秒级延时，用于时序微调
        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(10);
            break;

        // 初始化 GPIO 方向，硬件 I2C 模式下无需操作
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;

        // 以下 GPIO 消息在硬件 I2C 模式下不使用，直接返回成功
        case U8X8_MSG_GPIO_CS:
        case U8X8_MSG_GPIO_DC:
        case U8X8_MSG_GPIO_RESET:
            break;

        default:
            break;
    }
    return 0;
}

/**
 * @brief U8g2 I2C 字节回调函数
 *
 * U8g2 通过此回调逐字节向 I2C 总线发送数据。
 * 工作流程：
 *   1. U8x8 逐次调用 U8X8_MSG_BYTE_SEND，将数据逐字节写入 buffer
 *   2. 当一个传输块结束后，U8x8 调用 U8X8_MSG_BYTE_END_TRANSFER
 *   3. 此时将 buffer 中的数据通过 ESP-IDF I2C API 一次性发送
 */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
        // 每收到一个字节，暂存到 buffer 中
        case U8X8_MSG_BYTE_SEND:
            if (arg_ptr != NULL && i2c_buffer_idx < sizeof(i2c_buffer)) {
                // U8g2 一次可能发送 1~8 字节
                uint8_t *data = (uint8_t *)arg_ptr;
                for (uint8_t i = 0; i < arg_int && i2c_buffer_idx < sizeof(i2c_buffer); i++) {
                    i2c_buffer[i2c_buffer_idx++] = data[i];
                }
            }
            break;

        // 传输开始：重置 buffer 索引
        case U8X8_MSG_BYTE_INIT:
            i2c_buffer_idx = 0;
            break;

        // I2C 地址设置（已在外部通过 u8x8_SetI2CAddress 配置，此处忽略）
        case U8X8_MSG_BYTE_SET_DC:
            break;

        // 传输结束：通过 I2C 将 buffer 中的数据一次性发送给 OLED
        case U8X8_MSG_BYTE_END_TRANSFER:
        {
            if (i2c_buffer_idx == 0) {
                break;
            }

            // 构建 I2C 命令链
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            esp_err_t ret;

            // 起始信号
            ret = i2c_master_start(cmd);
            if (ret != ESP_OK) {
                i2c_cmd_link_delete(cmd);
                return 1;
            }

            // 发送从机地址 + 写标志
            // U8g2 传入的地址是 7-bit 原始地址（如 0x3C），需要左移1位变成 8-bit 地址
            ret = i2c_master_write_byte(cmd, (U8G2_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
            if (ret != ESP_OK) {
                i2c_cmd_link_delete(cmd);
                return 1;
            }

            // 发送 buffer 中所有数据
            ret = i2c_master_write(cmd, i2c_buffer, i2c_buffer_idx, true);
            if (ret != ESP_OK) {
                i2c_cmd_link_delete(cmd);
                return 1;
            }

            // 停止信号
            ret = i2c_master_stop(cmd);
            if (ret != ESP_OK) {
                i2c_cmd_link_delete(cmd);
                return 1;
            }

            // 执行命令链
            ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
            i2c_cmd_link_delete(cmd);

            // 重置 buffer 索引
            i2c_buffer_idx = 0;

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2C transfer failed: %d", ret);
                return 1;
            }
            break;
        }

        default:
            break;
    }
    return 0;
}
