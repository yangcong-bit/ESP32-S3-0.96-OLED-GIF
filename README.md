# ESP32-S3 0.96寸 OLED GIF 动画播放器

基于 ESP-IDF 框架，使用 U8g2 图形库驱动 SSD1306 OLED 屏幕播放 GIF 动画的完整项目。

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
│   └── u8g2/               # U8g2 图形库（需手动下载）
├── tools/
│   └── gif2u8g2.py         # GIF 转 U8g2 位图工具
├── sdkconfig               # ESP-IDF 配置文件
└── CMakeLists.txt          # 项目配置
```

## 快速开始

### 1. 安装依赖

```bash
# 安装 Pillow（用于 GIF 转换工具）
pip install Pillow
```

### 2. 下载 U8g2 库

```bash
cd components
git clone https://github.com/olikraus/u8g2.git
cd ..
```

### 3. 转换 GIF 文件

```bash
# 基本转换
python tools/gif2u8g2.py your_animation.gif

# 自动检测内容区域并适配尺寸（推荐）
python tools/gif2u8g2.py your_animation.gif --auto

# 反转颜色（白底→黑底）
python tools/gif2u8g2.py your_animation.gif --auto -i

# 转换 + 构建 + 烧录（一键完成）
python tools/gif2u8g2.py your_animation.gif --auto --build --flash
```

### 4. 构建与烧录

```bash
# 构建项目
idf.py build

# 烧录
idf.py -p COMx flash

# 监控串口输出
idf.py -p COMx monitor
```

## GIF 转换工具

`tools/gif2u8g2.py` 是一个通用的 GIF 转 U8g2 位图工具。

### 功能特性

- ✅ 自动检测 GIF 有效内容区域
- ✅ 保持宽高比，黑底居中填充
- ✅ 二值化处理，支持颜色反转
- ✅ 生成通用变量名的 C 头文件
- ✅ 支持通配符批量处理
- ✅ 可选自动构建和烧录

### 常用命令

```bash
# 不带参数：自动查找当前目录所有 GIF
python tools/gif2u8g2.py

# 通配符匹配
python tools/gif2u8g2.py *.gif

# 指定目录
python tools/gif2u8g2.py ./gifs/

# 自动适配 + 构建 + 烧录
python tools/gif2u8g2.py *.gif --auto --build --flash
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-w` | 目标宽度 | 128 |
| `-height` | 目标高度 | 64 |
| `-t` | 二值化阈值 | 128 |
| `-i` | 反转颜色 | 关闭 |
| `-a` / `--auto` | 自动检测内容区域 | 关闭 |
| `--build` | 转换后自动构建 | 关闭 |
| `--flash` | 构建后自动烧录 | 关闭 |

## 技术要点

### U8g2 HAL 适配

- I2C 通信：使用 ESP-IDF 原生 `driver/i2c.h` API
- 缓冲区策略：逐字节暂存，传输结束时一次性发送
- 时钟频率：400KHz 高速 I2C

### SSD1306 高刷新率

```c
// 0xD5: 设置时钟分频和振荡器频率
u8g2_SendF(&u8g2, "cc", 0xD5, 0xF0);
// 高4位(0xF)=振荡器最高，低4位(0x0)=分频最低
```

### GIF 播放原理

```
GIF 文件 → Python脚本 → SSD1306页格式头文件 → memcpy到u8g2显存 → SendBuffer
```

帧数据格式：128×64 像素 = 1024 字节/帧，按页存储（8页×128列）

## 依赖库

| 库 | 版本 | 说明 |
|----|------|------|
| ESP-IDF | v5.5.4 | 开发框架 |
| U8g2 | latest | 图形库 |
| Pillow | latest | Python GIF 处理 |

## 许可证

MIT License
