# A 版电气连接定义

本文档是生成版 KiCad PCB 的中文电气说明和接口约定。表中的网络名称与
`ai_mobile_reader_routed.kicad_pcb` 内嵌网络完全一致。

## ESP32-S3 信号分配

| 功能 | ESP32-S3 GPIO | PCB 网络名 |
|---|---:|---|
| 墨水屏忙信号 BUSY_N | 4 | EPD_BUSY |
| 墨水屏片选 | 5 | EPD_CS |
| SD 卡片选 | 13 | SD_CS |
| 墨水屏命令/数据选择 | 15 | EPD_DC |
| 墨水屏复位 | 16 | EPD_RST |
| 共用 SPI MOSI | 17 | SPI_MOSI |
| 共用 SPI 时钟 | 18 | SPI_SCK |
| 原生 USB D- | 19 | USB_D- |
| 原生 USB D+ | 20 | USB_D+ |
| SD 卡 MISO | 21 | SD_MISO |
| 上 / 主页 / 下按键 | 38 / 39 / 40 | BTN_UP / BTN_HOME / BTN_DOWN |
| 电源 / 返回按键 | 41 / 42 | BTN_POWER / BTN_BACK |
| 电池电压采样 | 1 | BAT_ADC |
| 下载启动控制 | 0 | BOOT |

## J3——墨水屏裸 FPC，24Pin、0.5 mm 间距

J3 使用 Hirose FH12-24S-0.5SH 类下接点连接器，安装在 PCB 背面，板上丝印
标有 Pin 1。FPC 金手指一面与屏幕正面方向一致，对应
`资料/墨水屏组件资料4.jpg`。

| 引脚 | 屏幕信号 | PCB 连接 |
|---:|---|---|
| 1 | NC | 不连接 |
| 2 | GDR | EPD_GDR |
| 3 | RESE | EPD_RESE |
| 4 | NC | 不连接 |
| 5 | VSH2 | EPD_VSH2 |
| 6 | GND | GND |
| 7 | GND | GND |
| 8 | BS | 接 GND，选择四线 SPI 模式 |
| 9 | BUSY_N | GPIO4 / EPD_BUSY，低电平表示忙 |
| 10 | RST_N | GPIO16 / EPD_RST |
| 11 | DC | GPIO15 / EPD_DC |
| 12 | CSB | GPIO5 / EPD_CS |
| 13 | SCL | GPIO18 / SPI_SCK |
| 14 | SDA | GPIO17 / SPI_MOSI |
| 15 | VDDIO | +3V3 |
| 16 | VCC | +3V3 |
| 17 | VSS | GND |
| 18 | VDD | EPD_VDD，使用 1 µF / 25 V 旁路电容 |
| 19 | VPP | 按用户提供的屏幕引脚表不连接 |
| 20 | VSH1 | EPD_VSH1 |
| 21 | VGH | PREVGH |
| 22 | VSL | EPD_VSL |
| 23 | VGL | PREVGL |
| 24 | VCOM | EPD_VCOM |

SSD1677 升压部分使用 L2 47 µH 电感、Q1 AO3400A 类 30 V NMOS、D1～D3
MBR0530 类肖特基二极管、R13 2.2 Ω 电阻及耐压 25 V 的储能电容。
TP5～TP8 引出 GDR、RESE、PREVGH 和 PREVGL，供首板调试测量。

## 电源、充电及 USB

- J1 是仅设备模式的 USB 2.0 Type-C 接口；CC1、CC2 分别使用 5.1 kΩ 下拉
  电阻，D+、D- 各串联 22 Ω，并配置 USBLC6-2SC6 ESD 防护器件；
- U2 BQ24074 负责单节锂电池充电和电源路径管理，`SYS` 为系统电源输出，
  输入及充电电流目标约为 500 mA；
- J2 连接带保护板的单节锂电池；装电池前必须核对实际 JST 正负极；
- U3 TPS63031 配合 L1 1.5 µH 电感，将 `SYS` 转换为固定 +3.3 V；
- R19/R20 为 1 MΩ / 330 kΩ 电池采样分压器，连接 GPIO1，C24 用于滤波。

## J4——SD 卡扩展板接口

两列引脚重复传输相同信号：

| J4 引脚 | 信号 |
|---|---|
| 1、2 | GND |
| 3、4 | +3V3 |
| 5、6 | SD_CS |
| 7、8 | SPI_MOSI |
| 9、10 | SPI_SCK |
| 11、12 | SD_MISO |
| 13、14 | 不连接 |
| 15、16 | GND |

## J5——外接按键接口

| J5 引脚 | 功能 |
|---:|---|
| 1 | 返回键 BACK |
| 2 | 电源键 POWER |
| 3 | 上键 UP |
| 4 | 主页键 HOME |
| 5 | 下键 DOWN |
| 6 | 公共 GND |

每路按键信号均通过 10 kΩ 电阻上拉到 +3.3 V，外接按键按下时将对应信号
与 GND 接通，因此按键为低电平有效。

## 板载控制按键

- SW1：ESP32-S3 硬件复位；
- SW2：按下时将 GPIO0 拉低，用于进入 ROM 下载模式；
- 固件可直接通过原生 USB 刷写，不需要额外的 USB 转串口芯片。
