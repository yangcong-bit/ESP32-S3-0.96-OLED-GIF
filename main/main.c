#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "driver/gpio.h"
#include "gif_frames.h"

static const char *TAG = "main";

// I2C 硬件配置
#define I2C_MASTER_SCL_IO   GPIO_NUM_17
#define I2C_MASTER_SDA_IO   GPIO_NUM_16

// GPIO18 控制引脚（置高电平）
#define GPIO18_CTRL_PIN     GPIO_NUM_18
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  400000

// U8g2 设备句柄
static u8g2_t u8g2;

/**
 * @brief I2C 主机初始化
 * 按照指定引脚配置 ESP32 硬件 I2C 主机
 */
static esp_err_t i2c_master_init(void)
{
    // 配置 GPIO18 输出高电平
    gpio_reset_pin(GPIO18_CTRL_PIN);
    gpio_set_direction(GPIO18_CTRL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO18_CTRL_PIN, 1);
    ESP_LOGI(TAG, "GPIO18 set HIGH");

    // 配置 I2C 参数
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %d", ret);
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "I2C master initialized");
    return ESP_OK;
}

/**
 * @brief OLED初始化
 * 配置U8g2驱动并点亮屏幕
 */
static void oled_init(void)
{
    ESP_ERROR_CHECK(i2c_master_init());

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
                                            u8g2_esp32_i2c_byte_cb,
                                            u8g2_esp32_gpio_and_delay_cb);

    u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C << 1);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    // 提高刷新率：设置 SSD1306 时钟分频和振荡器频率
    // 0xD5: 高4位=振荡器频率(0xF=最高), 低4位=分频比(0x0=最低)
    u8g2_SendF(&u8g2, "cc", 0xD5, 0xF0);

    ESP_LOGI(TAG, "OLED initialized (high refresh rate)");
}

/**
 * @brief GIF 动画播放任务
 * 
 * 直接将 SSD1306 页格式数据 memcpy 到 u8g2 显存缓冲区
 * 避免 DrawBitmap/DrawXBM 的格式不匹配问题
 */
static void gif_play_task(void *pvParameters)
{
    int current_frame = 0;
    const int frame_size = GIF_FRAME_SIZE;  // 每帧字节数
    
    // 帧率控制：20fps = 每帧间隔 50ms
    const TickType_t frame_delay = pdMS_TO_TICKS(50);

    ESP_LOGI(TAG, "GIF player started, %d frames", GIF_FRAME_COUNT);

    while (1) {
        // 获取 u8g2 显存缓冲区指针
        uint8_t *buf = u8g2_GetBufferPtr(&u8g2);
        
        // 直接拷贝帧数据（脚本已处理居中和白底填充）
        memcpy(buf, &gif_frame_data[current_frame * frame_size], frame_size);
        
        // 发送到屏幕
        u8g2_SendBuffer(&u8g2);
        
        // 切换到下一帧
        current_frame = (current_frame + 1) % GIF_FRAME_COUNT;
        
        // 帧率延时
        vTaskDelay(frame_delay);
    }
}

void app_main(void)
{
    // 1. 初始化 OLED
    oled_init();

    // 2. 创建 GIF 播放任务（栈空间 4096，优先级 5）
    xTaskCreate(gif_play_task, "gif_play_task", 4096, NULL, 5, NULL);
}
