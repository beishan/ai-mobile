#ifndef ESP_BOARD_CONFIG_H
#define ESP_BOARD_CONFIG_H

/*
 * ESP32-S3 N16R8 阅读器——硬件配置与接线总表
 * ================================================================
 * 本文件集中配置以下硬件模块：
 *   1. 4.26 英寸 480x800 黑白墨水屏（SSD1677，SPI）
 *   2. MicroSD 扩展板（SDSPI/FAT32，与墨水屏共享 SPI 总线）
 *   3. BACK / POWER / UP / HOME / DOWN 五个低电平有效按键
 *   4. SPI 频率、复位时序、BUSY 超时及 SD 卡挂载路径
 *
 * 修改硬件接线时，以本文件为唯一配置入口，并同步更新 README.md 和
 * docs/OPERATION_MANUAL.md。GPIO 编号表示 ESP32-S3 的 IO 编号，不是
 * 开发板排针的物理序号；接线前必须核对所购开发板丝印和原理图。
 *
 * 当前映射按实际排针范围（GPIO3~21、GPIO35~42、GPIO45~48 等）规划，
 * 主动避开 N16R8 常被 Flash/Octal PSRAM 占用的 GPIO26~37，也不使用
 * 原生 USB GPIO19/20、启动配置脚 GPIO0/3/45/46 和串口 RX/TX。
 * 板上标注的 RST 是 ESP32-S3 整板复位脚，不是普通 GPIO，不能连接
 * 墨水屏 RST；墨水屏复位信号在这里单独使用 GPIO16。
 */

/* ----------------------------------------------------------------
 * 模块一：SSD1677 墨水屏参数
 * ---------------------------------------------------------------- */
#define ESP_EPD_PANEL_NAME "4.26in 480x800 BW E-Ink"
#define ESP_EPD_DRIVER_IC "SSD1677"
#define ESP_EPD_WIDTH 480
#define ESP_EPD_HEIGHT 800

/*
 * 墨水屏 -> ESP32-S3 接线：
 *   BUSY_N -> GPIO4：屏幕输出，主控输入；低电平表示控制器忙
 *   RST  -> GPIO16 ：主控输出，低电平硬件复位；不要接开发板 RST 排针
 *   DC   -> GPIO15 ：主控输出，0=命令，1=数据
 *   CS   -> GPIO5  ：主控输出，低电平选中墨水屏
 *   SCK  -> GPIO18 ：SPI 时钟，同时连接 SD 扩展板 CLK
 *   SDA  -> GPIO17 ：SPI MOSI，同时连接 SD 扩展板 MOSI
 *   VCC  -> 3V3    ：屏幕模块电源，禁止误接 5V（除非模块明确支持）
 *   GND  -> GND    ：必须与 ESP32-S3、SD 扩展板共地
 *
 * SSD1677 只需要主控向屏幕发送数据，因此屏幕没有 MISO 接线。
 */
#define ESP_EPD_PIN_BUSY 4   /* EPD -> MCU：BUSY 状态输入 */
#define ESP_EPD_PIN_RST 16   /* MCU -> EPD：硬件复位；不是板载 RST 排针 */
#define ESP_EPD_PIN_DC 15    /* MCU -> EPD：命令/数据选择 */
#define ESP_EPD_PIN_CS 5     /* MCU -> EPD：独立片选，低有效 */
#define ESP_EPD_PIN_SCK 18   /* MCU -> EPD/SD：共享 SPI 时钟 */
#define ESP_EPD_PIN_SDA 17   /* MCU -> EPD/SD：共享 SPI MOSI */
#define ESP_EPD_BUSY_ACTIVE_LEVEL 0 /* OSPTEK 24Pin 排线标为 BUSY_N，低有效 */

/*
 * ----------------------------------------------------------------
 * 模块二：MicroSD 扩展板（SPI 模式）
 * ----------------------------------------------------------------
 * SD 扩展板 -> ESP32-S3 接线：
 *   CS   -> GPIO13 ：SD 独立片选，不能与墨水屏 CS(GPIO5)短接
 *   MOSI -> GPIO17 ：与墨水屏 SDA/MOSI 共用
 *   CLK  -> GPIO18 ：与墨水屏 SCK 共用
 *   MISO -> GPIO21 ：SD 向主控返回数据；墨水屏不连接此线
 *   VCC  -> 3V3    ：扩展板必须兼容 3.3V 逻辑和供电
 *   GND  -> GND    ：与主控及墨水屏共地
 *
 * SPI 共享规则：CLK/MOSI 可以并联，但每个模块必须有独立 CS。未被访问
 * 的模块保持 CS 高电平。GPIO21 用作 MISO，保留 GPIO19/20 给原生 USB。
 * SD 卡使用 FAT32，书籍目录为 /books，系统挂载后路径为 /sdcard/books。
 * 若扩展板没有自带上拉电阻，应按模块资料给 SD 信号线增加约 10k 上拉。
 */
#define ESP_SD_PIN_CS 13                /* MCU -> SD：独立片选，低有效 */
#define ESP_SD_PIN_MOSI ESP_EPD_PIN_SDA /* MCU -> SD：共享 GPIO17 */
#define ESP_SD_PIN_CLK ESP_EPD_PIN_SCK  /* MCU -> SD：共享 GPIO18 */
#define ESP_SD_PIN_MISO 21              /* SD -> MCU：SD 专用返回数据 */
#define ESP_SD_MOUNT_POINT "/sdcard"
#define ESP_SD_BOOK_DIRECTORY ESP_SD_MOUNT_POINT "/books"
#define ESP_SD_MAX_OPEN_FILES 5
#define ESP_SD_SPI_HZ_KHZ 10000         /* 10 MHz，优先保证杜邦线连接稳定 */

/* ----------------------------------------------------------------
 * 模块三：Wi-Fi 与 NTP 自动校时
 * ----------------------------------------------------------------
 * 首次使用前填写路由器名称和密码；留空则跳过联网，不影响离线阅读。
 * 时区使用中国标准时间 CST-8（UTC+8），NTP 成功后状态栏显示当前时间。
 * ESP32-S3 没有后备电池 RTC：断电后需要再次联网校时；如需断电保时可后续接 DS3231。 */
#define ESP_WIFI_SSID ""                /* 例如："MyHomeWiFi" */
#define ESP_WIFI_PASSWORD ""            /* 例如："your-password" */
#define ESP_NTP_SERVER "ntp.aliyun.com"
#define ESP_TIMEZONE "CST-8"
#define ESP_NTP_BOOT_TIMEOUT_MS 12000   /* 开机最多等待 12 秒，超时后继续离线启动 */

/* ----------------------------------------------------------------
 * 模块四：物理按键
 * ----------------------------------------------------------------
 * 每个按键一端接对应 GPIO，另一端接 GND，按下时读到低电平。
 * 五个按键使用 GPIO38~42，避开启动配置脚 GPIO0 和存储器 GPIO35~37。
 * 输入默认使用上拉；为提高抗干扰能力，建议每路外接 10k 上拉到 3V3。
 */
#define ESP_BUTTON_PIN_BACK 42  /* BACK -> GPIO42 -> 按键 -> GND（独立返回键） */
#define ESP_BUTTON_PIN_POWER 41 /* POWER -> GPIO41 -> 按键 -> GND */
#define ESP_BUTTON_PIN_UP 38    /* UP -> GPIO38 -> 按键 -> GND */
#define ESP_BUTTON_PIN_HOME 39  /* HOME -> GPIO39 -> 按键 -> GND */
#define ESP_BUTTON_PIN_DOWN 40  /* DOWN -> GPIO40 -> 按键 -> GND */
#define ESP_BUTTON_ACTIVE_LEVEL 0      /* 低电平表示按下 */
#define ESP_BUTTON_POLL_MS 20          /* 按键扫描周期 */
#define ESP_BUTTON_DEBOUNCE_MS 40      /* 两次稳定采样，兼顾机械消抖和短按响应 */
#define ESP_BUTTON_LONG_PRESS_MS 1200  /* POWER 长按判定时间 */

/* ----------------------------------------------------------------
 * 模块五：可选电池电压检测
 * ----------------------------------------------------------------
 * REV A PCB 使用 1M/330k 分压接 GPIO1；4.2V 电池在 ADC 脚约为 1.04V。
 * NUMERATOR/DENOMINATOR 表示从 ADC 电压还原电池电压的 133/33 比例。 */
#define ESP_BATTERY_ADC_GPIO 1
#define ESP_BATTERY_DIVIDER_NUMERATOR 133
#define ESP_BATTERY_DIVIDER_DENOMINATOR 33
#define ESP_BATTERY_EMPTY_MV 3300
#define ESP_BATTERY_FULL_MV 4200

/* ----------------------------------------------------------------
 * 模块六：共享 SPI 与 SSD1677 时序
 * ----------------------------------------------------------------
 * SPI2_HOST 同时服务墨水屏和 SD 卡；设备通过各自 CS 避免总线冲突。
 */
#define ESP_EPD_SPI_HOST SPI2_HOST
#define ESP_EPD_SPI_HZ 10000000         /* 墨水屏 SPI：10 MHz */
#define ESP_EPD_RESET_LOW_MS 20         /* RST 保持低电平时间 */
#define ESP_EPD_RESET_HIGH_MS 200       /* RST 拉高后的稳定等待 */
#define ESP_EPD_BUSY_TIMEOUT_MS 60000   /* 全刷最长等待：60 秒 */
#define ESP_EPD_PARTIAL_REFRESH_LIMIT 12 /* 连续局刷到达阈值后自动全刷；设为 0 可禁用 */

/* 仅用于启动日志显示；实际供电必须接开发板 3V3 和 GND。 */
#define ESP_EPD_POWER_GND "GND"
#define ESP_EPD_POWER_VCC "3V3"

#endif
