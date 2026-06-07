# ESP32-S3 0.96寸 OLED GIF 动画播放器

基于 ESP-IDF 框架，使用 U8g2 图形库驱动 SSD1306 OLED 屏幕播放 GIF 动画的完整项目。

当前播放动画：**呆猫八条**（白底黑线，居中显示）

## 硬件配置

| 组件 | 规格 |
|------|------|
| 开发板 | 嘉立创 ESP32-S3 |
| Flash | 256Mbit (32MB) |
| PSRAM | ESP-PSRAM64H (8MB) |
| OLED | 0.96寸 SSD1306 128x64 I2C |

### 引脚定义

| 引脚 | GPIO | 功能 |
|------|------|------|
| SCL | GPIO17 | I2C 时钟线 |
| SDA | GPIO16 | I2C 数据线 |
| Ctrl | GPIO18 | 控制引脚（置高电平） |

## 项目结构

```
ESP32_0.96OLED/
├── main/
│   ├── main.c              # 主程序（OLED初始化 + GIF播放）
│   ├── gif_frames.h        # 自动生成的动画帧数据
│   ├── u8g2_esp32_hal.h    # U8g2 HAL 适配头文件
│   ├── u8g2_esp32_hal.c    # U8g2 I2C/延时回调实现
│   ├── ssd1306.h           # SSD1306 驱动头文件
│   ├── ssd1306.c           # SSD1306 驱动实现
│   └── CMakeLists.txt      # 主组件配置
├── components/
│   └── u8g2/               # U8g2 图形库（Git Submodule，自动拉取）
├── tools/
│   └── gif2u8g2.py         # GIF 转 U8g2 位图工具
├── 表情包/
│   ├── 呆猫八条.gif        # 当前使用的动画
│   └── 月薪喵.gif
├── sdkconfig               # ESP-IDF 配置文件
├── CMakeLists.txt          # 项目配置
└── README.md
```

## 快速开始

### 1. 克隆项目（含 U8g2 子模块）

```bash
git clone --recurse-submodules https://gitee.com/yangcongjiang/ESP32_S3_0.96_OLED_GIF.git
cd ESP32_S3_0.96_OLED_GIF
```

### 2. 安装依赖

```bash
pip install Pillow
```

### 3. 转换 GIF 文件

```bash
# 推荐命令：转换 呆猫八条，白底填充
python tools/gif2u8g2.py "表情包/呆猫八条.gif" -w 128 -height 64 -bg 1

# 自动检测内容区域
python tools/gif2u8g2.py "表情包/呆猫八条.gif" --auto

# 黑底填充
python tools/gif2u8g2.py "表情包/呆猫八条.gif" -w 128 -height 64 -bg 0

# 反转颜色
python tools/gif2u8g2.py "表情包/呆猫八条.gif" -w 128 -height 64 -i
```

### 4. 构建与烧录

```bash
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

## GIF 转换工具

`tools/gif2u8g2.py` 将 GIF 转换为 U8g2 可读的 C 头文件。

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-w` | 目标宽度 | 128 |
| `-height` | 目标高度 | 64 |
| `-t` | 二值化阈值 | 128 |
| `-i` | 反转颜色 | 关闭 |
| `-bg` | 填充区域颜色 (0=黑, 1=白) | 1（白色） |
| `-a` / `--auto` | 自动检测内容区域 | 关闭 |
| `--build` | 转换后自动构建 | 关闭 |
| `--flash` | 构建后自动烧录 | 关闭 |

### 功能特性

- ✅ 自动检测 GIF 有效内容区域（`--auto`）
- ✅ 保持宽高比，居中放置，**填充区域可选黑白**（`-bg`）
- ✅ 二值化处理，支持颜色反转（`-i`）
- ✅ 生成通用变量名的 C 头文件
- ✅ 支持通配符批量处理（`*.gif`）
- ✅ 可选自动构建和烧录

## 技术要点

### U8g2 HAL 适配

- I2C 通信：ESP-IDF 原生 `driver/i2c.h`
- 缓冲区：逐字节暂存，`U8X8_MSG_BYTE_END_TRANSFER` 时一次性发送
- 时钟频率：400KHz

### SSD1306 高刷新率

```c
u8g2_SendF(&u8g2, "cc", 0xD5, 0xF0);
```

### GIF 播放原理

```
GIF → gif2u8g2.py → gif_frames.h (SSD1306页格式) → memcpy到u8g2显存 → SendBuffer
```

### 播放流程

1. Python 脚本将 GIF 每帧缩放到 128×64，二值化，居中放置在白底画布上
2. 生成 `gif_frames.h`，包含 `gif_frame_data[]` 数组和宏定义
3. ESP32 上电后 FreeRTOS 任务循环：`ClearBuffer → memcpy帧数据 → SendBuffer`
4. 帧率 20fps（50ms/帧）

帧数据格式：128×64 像素 = 1024 字节/帧，按页存储（8页×128列）

## 依赖库

| 库 | 版本 | 说明 |
|----|------|------|
| ESP-IDF | v5.5.4 | 开发框架 |
| U8g2 | latest | 图形库 |
| Pillow | latest | Python GIF 处理 |

## 许可证

MIT License
