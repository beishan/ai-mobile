# ESP32 黑白高刷墨水屏阅读器 - 产品需求规格说明

**版本** v2.0  
**平台** ESP32 N16R8 + PlatformIO (ESP-IDF) + 4.26 寸 480×800 黑白高刷 E-Ink + SSD677 + SPI  
**定位** 以阅读为核心的便携式黑白墨水屏设备。

## 1. 硬件规格

### 1.1 主控

| 项目 | 规格 |
|------|------|
| 型号 | ESP32 N16R8 |
| Flash | 16MB |
| PSRAM | 8MB |
| 主频 | 240MHz |
| WiFi | 802.11 b/g/n 2.4GHz |
| 开发框架 | PlatformIO + ESP-IDF |

### 1.2 显示屏

| 项目 | 规格 |
|------|------|
| 尺寸 | 4.26 inch |
| 分辨率 | 480×800 px |
| 显示 | 黑白 |
| 刷新特性 | 高刷，优先支持阅读翻页和菜单切换 |
| 驱动 IC | SSD677 |
| 接口 | SPI |
| 帧缓冲 | 单 1bpp 黑白平面，480×800/8 = 48,000 bytes |

## 2. 接线指南

SSD677 E-Ink 屏使用 SPI 接口。当前代码中的唯一接线配置源为 `src/platform/esp_board_config.h`。

| 屏幕引脚 | 功能 | ESP32 |
|----------|------|-------|
| VCC | 电源 | 3V |
| GND | 地 | GND |
| SDA/DIN | SPI MOSI | GPIO23 |
| SCK/CLK | SPI 时钟 | GPIO18 |
| CS | 片选 | GPIO5 |
| DC | 数据/命令 | GPIO17 |
| RST | 复位 | GPIO16 |
| BUSY | 忙信号 | GPIO4 |

四个物理按键保持当前设计：

| Button | ESP32 pin |
|--------|-----------|
| POWER | GPIO0 |
| UP | GPIO35 |
| HOME | GPIO34 |
| DOWN | GPIO39 |

POWER 短按保持返回/退出行为；POWER 长按进入省电休眠路径。

## 3. 系统架构

项目分为共享应用层和平台层：

- `src/gfx`: 480×800 黑白 framebuffer 与基础绘图。
- `src/font`: 点阵字体渲染。
- `src/app`: 页面状态机、阅读进度、设置状态。
- `src/ui`: 首页、阅读、天气、日历、英语、设置、关于页面渲染。
- `src/platform`: SDL/PPM 仿真、ESP32 输入、SSD677 SPI 显示适配。

## 4. 功能模块

### 4.1 首页

首页固定为六个入口：

```text
阅读 / 天气 / 日历
英语 / 设置 / 关于
```

首页不包含游戏入口。UP/DOWN 移动选择，HOME 进入，POWER 从子页返回首页。

### 4.2 阅读模块

- 书架显示三本 mock 书，保留每本书的阅读进度。
- 打开书籍后恢复该书的当前页。
- 阅读页顶部显示书名和页码。
- 阅读页底部保持留白，避免干扰正文。
- HOME 打开阅读菜单。
- 菜单支持 `继续阅读 / 查看目录 / 添加书签 / 退出到书架`。
- 目录弹层支持章节选择和跳转。
- 书签按书籍记录，当前仿真阶段保存在运行时状态中。
- 字号和行间距由设置模块即时影响阅读正文排版。

### 4.3 天气模块

- 当前仿真阶段使用北京、上海、广州三组 mock 数据。
- UP/DOWN 切换城市。
- HOME 在 WiFi 已连接时刷新天气状态。
- WiFi 关闭时保留缓存状态并显示离线数据年龄。

### 4.4 日历模块

- 显示月历、选中日期、农历/节气摘要 mock 信息。
- UP/DOWN 切换月份。
- HOME 打开或关闭日期详情。
- 日期详情打开后，UP/DOWN 按周移动选中日期。

### 4.5 英语学习模块

- 显示单词正面和释义反面。
- 正面 UP/DOWN 切换单词，HOME 翻面。
- 反面 UP 标记复习，DOWN 标记认识，并进入下一词。
- 页面显示认识/复习统计与答题状态点。

### 4.6 设置模块

- 字体大小。
- 行间距。
- WiFi 开关。
- 城市选择。
- 省电模式。

设置修改应立即反映到共享状态；阅读相关设置立即影响阅读页渲染。

### 4.7 关于模块

关于页显示：

- 项目名称。
- 固件/仿真版本。
- ESP32 N16R8。
- 4.26 寸 480×800 黑白高刷屏。
- SSD677 SPI。

## 5. 黑白显示规范

项目只使用两种逻辑颜色：

| 颜色 | 用途 |
|------|------|
| 白色 | 背景、留白、反白文字 |
| 黑色 | 正文、图标、边框、进度、选中态、警告强调 |

选中态使用反白、黑色短线、双线框或加粗图形表达。不得使用红色语义。

## 6. 帧缓冲和 SSD677 适配

共享 framebuffer 尺寸必须为 480×800。

```c
#define GFX_WIDTH 480
#define GFX_HEIGHT 800
#define EPD_FRAME_BYTES ((GFX_WIDTH * GFX_HEIGHT) / 8)

typedef struct {
    unsigned char bw[EPD_FRAME_BYTES];
} epd_frame_t;
```

打包规则：

- 白色像素对应 bit = 1。
- 黑色像素对应 bit = 0。
- 每字节从高位到低位对应从左到右的 8 个像素。

SSD677 初始化、窗口设置、数据写入、刷新触发、休眠命令由 `src/platform/esp_display.c` 承接。当前阶段先完成 GPIO/SPI 初始化、frame packing、日志验证；后续接入供应商 datasheet 的具体命令表。

## 7. 仿真器要求

### 7.1 PPM 仿真

`reader_sim` 输出 `out/frame.ppm`，尺寸必须为 480×800。

### 7.2 SDL 仿真

`reader_sim_sdl` 使用同一 framebuffer，键盘映射如下：

| Key | Button |
|-----|--------|
| Up / w | UP |
| Down / s | DOWN |
| Enter / Space / h | HOME |
| Esc / Backspace / p | POWER |
| q | Quit |

SDL smoke 模式：

```bash
SDL_VIDEODRIVER=dummy ./reader_sim_sdl --smoke
```

## 8. 非功能需求

- 共享代码必须同时服务仿真器和 ESP32 固件。
- 页面渲染不得依赖红色或三色屏行为。
- 所有页面必须能在 480×800 framebuffer 上非空渲染。
- 首页不得出现游戏入口。
- 文档、测试、代码中的当前目标必须统一为 4.26 寸 480×800 黑白 SSD677 SPI。

## 9. 后续开发

- 接入 SSD677 真实初始化和刷新命令。
- 增加真实文件读取、TXT 分页、GBK/UTF-8 检测。
- 增加 SD/NVS 持久化阅读进度和书签。
- 改进 480×800 竖屏布局密度。
- 增加电量检测。
