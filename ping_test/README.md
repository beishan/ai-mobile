# ESP32 + 鱼鹰光电 4.26 寸电子纸 SSD1677 测试项目

这是一个 PlatformIO/Arduino 测试工程，用于 ESP32 驱动鱼鹰光电 4.26 寸黑白双色电子纸墨水屏。

屏幕参数按已知资料配置：

- 尺寸：4.26 inch
- 分辨率：800 x 480，代码中旋转为 480 x 800 竖屏绘制
- 颜色：黑白双色
- 接口：SPI
- Driver IC：SSD1677
- 帧缓冲：800 x 480 / 8 = 48000 字节

## 工程内容

- `src/FishEpd426SSD1677.cpp`：简化 SSD1677 SPI 全屏刷新驱动
- `include/FishEpd426SSD1677.h`：驱动类和引脚配置
- `src/main.cpp`：测试页面，包含中文、英文、线条、块填充、圆形图案和参数文字
- `platformio.ini`：ESP32 + Arduino 构建配置

## 默认 ESP32 接线

请按你的转接板实际标注修改 `src/main.cpp` 里的 `FishEpdPins pins`。

| 屏幕/转接板信号 | ESP32 默认引脚 | 说明 |
| --- | ---: | --- |
| SCL / SCK | GPIO18 | SPI 时钟 |
| SDA / MOSI | GPIO23 | SPI 数据输出 |
| CS | GPIO5 | 片选 |
| DC | GPIO26 | 数据/命令 |
| RST / RESET | GPIO27 | 复位 |
| BUSY | GPIO4 | 忙信号，当前代码按高电平忙处理 |
| VCC | 3.3V | 不建议直接连接 5V 逻辑 |
| GND | GND | 地 |

资料里的 24Pin 裸屏 FPC 不是 ESP32 可直接驱动的接口，通常需要电子纸转接板或升压驱动板。ESP32 应连接转接板上引出的 SPI、RST、BUSY、DC、CS 等信号。

## 编译和烧录

在本目录执行：

```bash
pio run -t upload
pio device monitor
```

如果使用 VS Code + PlatformIO，直接打开本文件夹，然后选择 `Upload`。

## 测试画面

刷新后应看到：

- 顶部黑底白字标题
- 中文：鱼鹰光电、电子纸测试、黑白双色墨水屏
- 英文：Hello ESP32、Black / White、SPI E-Paper、SSD1677
- 黑白线条、填充块、圆形图案
- 刷新流程和构建时间

## 常见调整

1. 屏幕一直不动

   检查 BUSY 引脚电平。有些转接板 BUSY 为高忙，有些为低忙。当前代码按高电平忙处理；如果串口反复打印 `EPD BUSY timeout`，可以把 `src/main.cpp` 里的 `pins.busyActiveLevel = HIGH;` 改成 `LOW` 再试。

2. 显示反色

   修改 `FishEpd426SSD1677::drawPixel()` 中黑白位的写入逻辑，或把 `clear(EPD_WHITE)` 改成 `clear(EPD_BLACK)` 做验证。

3. 方向不对

   修改 `src/main.cpp` 中的 `epd.setRotation(0)`，可尝试 `0/1/2/3`。

4. 只显示花屏或部分显示

   重点检查分辨率、RAM X/Y 窗口和转接板驱动 IC 是否确认是 SSD1677。不同批次屏幕的 SSD1677 初始化表可能略有差异，必要时用厂家 demo 里的 init table 替换 `initPanel()`。

5. 中文不显示

   确认 PlatformIO 已安装 `U8g2_for_Adafruit_GFX`，并且源文件保持 UTF-8 编码。

## 说明

这个工程优先用于点亮和验证内容显示。电子纸驱动 IC 的电源、波形 LUT、温度设置在不同厂家屏幕上可能不同。如果屏幕不能正常刷新，最可靠的做法是向鱼鹰光电索取该型号 SSD1677 的 Arduino 或 C 初始化序列，然后替换 `FishEpd426SSD1677::initPanel()`。
