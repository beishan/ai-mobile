# AI 移动阅读器 PCB——A 版

本目录保存 4.26 英寸竖屏墨水阅读器的硬件设计，主要组成如下：

- ESP32-S3-WROOM-1-N16R8 模组；
- OSPTEK 4.26 英寸、480 × 800 黑白墨水屏，SSD1677 控制器，24Pin、
  0.5 mm 间距 FPC；
- 一节带保护板的 3.7 V 锂离子/锂聚合物电池，使用 JST-PH-2 接口；
- BQ24074 电源路径充电芯片及 TPS63031 3.3 V 升降压稳压器；
- ESP32-S3 原生 USB，通过 USB-C 接口连接电脑和刷写固件；
- 外接五按键接口及板载 MicroSD 卡座。

## 机械设计约定

- 四层板，厚度 1.6 mm，尺寸 58 × 102 mm，设计为安装在屏幕后方；
- 屏幕排线金手指面与屏幕正面方向一致；J3 安装在 PCB 背面，使用下接点
  连接器，丝印已标出 Pin 1 及金手指方向；
- USB-C 从 PCB 底边伸出，MicroSD 卡从右侧插入，按键接口位于右侧；
- 当前安装位置适合作为样机默认布局，尚未针对最终外壳进行结构定型。

## 安全及首次上电检查

- 只能使用带保护板的单节锂电池；
- 通电状态下禁止插拔墨水屏 FPC；
- 安装屏幕前，确认 J3 的 6、7、8、17 脚均为 GND，并用实物再次核对
  连接器接触面和 Pin 1 方向；
- 第一次上电使用带限流功能的实验电源，先确认 3.3 V 正常，再安装屏幕；
- 屏幕刷新时通过 TP5～TP8 检查 GDR、RESE、PREVGH 和 PREVGL；
- 墨水屏高压电路采用 SSD1677 参考拓扑，C12～C23 必须使用 X5R/X7R、
  耐压不低于 25 V 的电容。

## 文件说明

- `ai_mobile_reader_routed.kicad_pcb`：已经完成布线的 KiCad PCB，用于检查和生产；
- `ai_mobile_reader.kicad_pcb`：自动生成的未布线布局，作为可重复生成的布线输入；
- `SCHEMATIC.md`：完整引脚表、电气连接及接口定义；
- `bom.csv`：器件清单；
- `manufacturing/`：Gerber、钻孔、贴片坐标和生产说明；
- `reports/`：DRC 报告及 PCB 正反面渲染图。

## 重新生成 PCB

生成脚本依赖 KiCad 自带的 Python 和 `pcbnew` 模块。本机命令如下：

```sh
PY='/Volumes/KiCad/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3'
PYTHONPATH='/Volumes/KiCad/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/lib/python3.9/site-packages' \
DYLD_LIBRARY_PATH='/Volumes/KiCad/KiCad/KiCad.app/Contents/Frameworks' \
"$PY" hardware/generate_board.py
```

布线、DRC 检查及生产文件导出要求见 `manufacturing/README.md`。
