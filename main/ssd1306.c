#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "ssd1306";

static esp_err_t ssd1306_i2c_master_write(uint8_t control, const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t ret = i2c_master_start(cmd);
    if (ret != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return ret;
    }

    // 发送从机地址 + 写标志
    ret = i2c_master_write_byte(cmd, (SSD1306_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    if (ret != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return ret;
    }

    // 发送控制字节：命令或数据
    ret = i2c_master_write_byte(cmd, control, true);
    if (ret != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return ret;
    }

    // 发送实际数据
    if (len > 0 && data != NULL) {
        ret = i2c_master_write(cmd, data, len, true);
        if (ret != ESP_OK) {
            i2c_cmd_link_delete(cmd);
            return ret;
        }
    }

    ret = i2c_master_stop(cmd);
    if (ret != ESP_OK) {
        i2c_cmd_link_delete(cmd);
        return ret;
    }

    ret = i2c_master_cmd_begin(SSD1306_I2C_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t ssd1306_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SSD1306_I2C_SDA_PIN,
        .scl_io_num = SSD1306_I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = SSD1306_I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(SSD1306_I2C_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed (%d)", ret);
        return ret;
    }

    ret = i2c_driver_install(SSD1306_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed (%d)", ret);
        return ret;
    }

    return ESP_OK;
}

esp_err_t ssd1306_write_command(uint8_t cmd)
{
    return ssd1306_i2c_master_write(SSD1306_CONTROL_CMD, &cmd, 1);
}

esp_err_t ssd1306_write_data(const uint8_t *data, size_t len)
{
    return ssd1306_i2c_master_write(SSD1306_CONTROL_DATA, data, len);
}

esp_err_t ssd1306_set_cursor(uint8_t page, uint8_t column)
{
    if (page >= SSD1306_PAGE_NUM || column >= SSD1306_WIDTH) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmds[3];
    cmds[0] = 0xB0 | page;                  // 页面地址
    cmds[1] = 0x00 | (column & 0x0F);      // 列地址低位
    cmds[2] = 0x10 | ((column >> 4) & 0x0F); // 列地址高位

    return ssd1306_i2c_master_write(SSD1306_CONTROL_CMD, cmds, sizeof(cmds));
}

esp_err_t ssd1306_clear(void)
{
    uint8_t buffer[SSD1306_WIDTH];
    memset(buffer, 0x00, sizeof(buffer));

    for (uint8_t page = 0; page < SSD1306_PAGE_NUM; page++) {
        esp_err_t ret = ssd1306_set_cursor(page, 0);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = ssd1306_write_data(buffer, sizeof(buffer));
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t ssd1306_fill(uint8_t pattern)
{
    uint8_t buffer[SSD1306_WIDTH];
    memset(buffer, pattern, sizeof(buffer));

    for (uint8_t page = 0; page < SSD1306_PAGE_NUM; page++) {
        esp_err_t ret = ssd1306_set_cursor(page, 0);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = ssd1306_write_data(buffer, sizeof(buffer));
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t ssd1306_init(void)
{
    esp_err_t ret = ssd1306_i2c_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // SSD1306 初始化序列
    const uint8_t init_cmds[] = {
        0xAE, // 关闭显示
        0xD5, 0x80, // 设置时钟分频因子
        0xA8, 0x3F, // 设置复用率 1/64
        0xD3, 0x00, // 设置显示偏移
        0x40, // 设置起始行
        0x8D, 0x14, // 启用充电泵
        // 修改为页寻址模式
        0x20, 0x02, // 设置内存地址模式为页模式 (Page Addressing Mode)
        0xA1, // 段重映射
        0xC8, // COM 输出扫描方向反转
        0xDA, 0x12, // 设置 COM 引脚硬件配置
        0x81, 0xCF, // 设置对比度
        0xD9, 0xF1, // 设置预充电周期
        0xDB, 0x40, // 设置 VCOMH 去耦电压
        0xA4, // 关闭全屏点亮，遵循 RAM 内容
        0xA6, // 正常显示
        0xAF, // 打开显示
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        ret = ssd1306_write_command(init_cmds[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ssd1306_write_command failed at index %d (%d)", (int)i, ret);
            return ret;
        }
    }

    // 初始化结束后清屏，保证显示空白
    vTaskDelay(pdMS_TO_TICKS(50));
    ret = ssd1306_clear();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ssd1306_clear failed (%d)", ret);
        return ret;
    }

    ESP_LOGI(TAG, "SSD1306 init finished");
    return ESP_OK;
}
