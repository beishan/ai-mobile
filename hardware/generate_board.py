#!/usr/bin/env python3
"""Generate the first-revision AI Mobile reader PCB with KiCad's pcbnew API.

The board intentionally uses only footprints shipped with KiCad so the generated
project is self-contained after saving.  Run with the Python bundled in KiCad;
see hardware/README.md for the exact command.
"""

from pathlib import Path
import csv
import os
import pcbnew


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "ai_mobile_reader.kicad_pcb"
KICAD_ROOT = Path(os.environ.get(
    "KICAD_FOOTPRINT_ROOT",
    "/Volumes/KiCad/KiCad/KiCad.app/Contents/SharedSupport/footprints",
))


def mm(value):
    return pcbnew.FromMM(float(value))


def point(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


board = pcbnew.BOARD()
board.SetCopperLayerCount(4)
board.GetDesignSettings().SetBoardThickness(mm(1.6))


NETS = {}


def net(name):
    if not name:
        return None
    if name not in NETS:
        item = pcbnew.NETINFO_ITEM(board, name)
        board.Add(item)
        NETS[name] = item
    return NETS[name]


for name in (
    "GND", "+3V3", "USB_VBUS", "SYS", "VBAT", "USB_D+", "USB_D-",
    "USB_D+_MCU", "USB_D-_MCU", "CC1", "CC2", "EN", "BOOT",
    "EPD_BUSY", "EPD_RST", "EPD_DC", "EPD_CS", "SPI_SCK", "SPI_MOSI",
    "SD_CS", "SD_MISO", "SD_DAT1", "SD_DAT2", "BTN_BACK", "BTN_POWER", "BTN_UP", "BTN_HOME",
    "BTN_DOWN", "BAT_ADC", "BQ_CHG", "BQ_ILIM", "BQ_TMR", "BQ_ITERM",
    "BQ_ISET", "BQ_TS", "REG_L1", "REG_L2", "REG_VINA", "BOOST_SW",
    "BOOST_FLY", "PREVGH", "PREVGL", "EPD_GDR", "EPD_RESE", "EPD_VDD",
    "EPD_VSH2", "EPD_VSH1", "EPD_VSL", "EPD_VCOM", "LED_CHG_A",
):
    net(name)


def load_fp(lib, name, ref, value, x, y, rotation=0, side="F", pads=None):
    fp = pcbnew.FootprintLoad(str(KICAD_ROOT / f"{lib}.pretty"), name)
    if fp is None:
        raise RuntimeError(f"Unable to load {lib}:{name}")
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(point(x, y))
    fp.SetOrientationDegrees(rotation)
    board.Add(fp)
    if side == "B":
        fp.Flip(point(x, y), False)
    for pad in fp.Pads():
        number = pad.GetNumber()
        if pads and number in pads and pads[number]:
            pad.SetNet(net(pads[number]))
    return fp


def passive(kind, ref, value, x, y, a, b, rotation=0, size="0603"):
    if kind == "R":
        lib, name = "Resistor_SMD", f"R_{size}_1608Metric" if size == "0603" else "R_0805_2012Metric"
    elif kind == "C":
        lib, name = "Capacitor_SMD", f"C_{size}_1608Metric" if size == "0603" else "C_0805_2012Metric"
    else:
        raise ValueError(kind)
    return load_fp(lib, name, ref, value, x, y, rotation, pads={"1": a, "2": b})


# Board outline: fits behind the 62.37 x 105.33 mm portrait EPD.
for x1, y1, x2, y2 in ((1, 1, 59, 1), (59, 1, 59, 103), (59, 103, 1, 103), (1, 103, 1, 1)):
    edge = pcbnew.PCB_SHAPE(board)
    edge.SetShape(pcbnew.SHAPE_T_SEGMENT)
    edge.SetStart(point(x1, y1))
    edge.SetEnd(point(x2, y2))
    edge.SetLayer(pcbnew.Edge_Cuts)
    edge.SetWidth(mm(0.05))
    board.Add(edge)


# Bottom mounting holes. The top edge is deliberately hole-free because the
# WROOM antenna keepout spans almost the full board width.
for idx, (x, y) in enumerate(((4, 99), (56, 99)), start=1):
    load_fp("MountingHole", "MountingHole_2.5mm_Pad_Via", f"H{idx}", "M2.5", x, y, pads={"1": "GND"})


# MCU module. Antenna points toward the upper board edge and has no copper under it.
esp_pads = {
    "1": "GND", "2": "+3V3", "3": "EN", "4": "EPD_BUSY", "5": "EPD_CS",
    "8": "EPD_DC", "9": "EPD_RST", "10": "SPI_MOSI", "11": "SPI_SCK",
    "13": "USB_D-", "14": "USB_D+", "21": "SD_CS", "23": "SD_MISO",
    "31": "BTN_UP", "32": "BTN_HOME", "33": "BTN_DOWN", "34": "BTN_POWER",
    "35": "BTN_BACK", "38": "BOOT", "39": "BAT_ADC", "40": "GND", "41": "GND",
}
u1 = load_fp("RF_Module", "ESP32-S3-WROOM-1", "U1", "ESP32-S3-WROOM-1-N16R8", 30, 21, pads=esp_pads)
passive("C", "C1", "10uF/6.3V", 17.5, 22, "+3V3", "GND", rotation=90, size="0805")
passive("C", "C2", "100nF", 19.5, 22, "+3V3", "GND", rotation=90)
passive("R", "R1", "10k", 14, 26, "+3V3", "EN")
passive("C", "C3", "1uF", 18, 26, "EN", "GND")
passive("R", "R2", "10k", 42.5, 27, "+3V3", "BOOT")
load_fp("Button_Switch_SMD", "Panasonic_EVQPUJ_EVQPUA", "SW1", "RESET", 8, 34, pads={"1": "EN", "2": "GND"})
load_fp("Button_Switch_SMD", "Panasonic_EVQPUJ_EVQPUA", "SW2", "BOOT", 48, 27, pads={"1": "BOOT", "2": "GND"})


# USB-C native USB programming interface.
usb_pads = {
    "A1": "GND", "A12": "GND", "B1": "GND", "B12": "GND", "SH": "GND",
    "A4": "USB_VBUS", "A9": "USB_VBUS", "B4": "USB_VBUS", "B9": "USB_VBUS",
    "A5": "CC1", "B5": "CC2", "A6": "USB_D+_MCU", "B6": "USB_D+_MCU",
    "A7": "USB_D-_MCU", "B7": "USB_D-_MCU",
}
load_fp("Connector_USB", "USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal", "J1", "USB-C", 30, 100.2, pads=usb_pads)
passive("R", "R3", "5.1k", 23, 90, "CC1", "GND", rotation=90)
passive("R", "R4", "5.1k", 26, 90, "CC2", "GND", rotation=90)
passive("R", "R5", "22R", 32, 92, "USB_D+_MCU", "USB_D+", rotation=90)
passive("R", "R6", "22R", 35, 92, "USB_D-_MCU", "USB_D-", rotation=90)
load_fp("Package_TO_SOT_SMD", "SOT-23-6", "D4", "USBLC6-2SC6", 39, 95, pads={"1": "USB_D+_MCU", "2": "GND", "3": "USB_D-_MCU", "4": "USB_D-", "5": "USB_VBUS", "6": "USB_D+"})


# Battery charger with power path, configured for a 500 mA USB input/charge target.
bq_pads = {
    "1": "BQ_TS", "2": "VBAT", "3": "VBAT", "4": "GND", "5": "GND",
    "6": "SYS", "7": "BQ_CHG", "8": "GND", "9": "BQ_CHG", "10": "SYS",
    "11": "SYS", "12": "BQ_ILIM", "13": "USB_VBUS", "14": "BQ_TMR",
    "15": "BQ_ITERM", "16": "BQ_ISET", "17": "GND",
}
load_fp("Package_DFN_QFN", "VQFN-16-1EP_3x3mm_P0.5mm_EP1.6x1.6mm", "U2", "BQ24074RGT", 11, 87, pads=bq_pads)
load_fp("Connector_JST", "JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical", "J2", "1S_PROTECTED_LIPO", 5, 80, rotation=90, pads={"1": "VBAT", "2": "GND"})
passive("C", "C4", "1uF/10V", 8, 92, "USB_VBUS", "GND", size="0805")
passive("C", "C5", "4.7uF/10V", 5.5, 86, "VBAT", "GND", size="0805")
passive("C", "C6", "4.7uF/10V", 16.5, 87, "SYS", "GND", size="0805")
passive("R", "R7", "10k", 7, 84, "BQ_TS", "GND")
passive("R", "R8", "3.24k 1%", 13, 82, "BQ_ILIM", "GND")
passive("C", "C7", "100nF", 16, 82, "BQ_TMR", "GND")
passive("R", "R9", "1.8k 1%", 10, 80, "BQ_ITERM", "GND")
passive("R", "R10", "1.78k 1%", 13, 79, "BQ_ISET", "GND")
load_fp("LED_SMD", "LED_0603_1608Metric", "D5", "CHARGE_AMBER", 17.5, 94, pads={"1": "BQ_CHG", "2": "LED_CHG_A"})
passive("R", "R11", "1k", 21, 94, "LED_CHG_A", "USB_VBUS")


# 3.3 V buck-boost regulator, fixed output TPS63031.
reg_pads = {"1": "+3V3", "2": "REG_L2", "3": "GND", "4": "REG_L1", "5": "SYS", "6": "SYS", "7": "SYS", "8": "REG_VINA", "9": "GND", "10": "+3V3", "11": "GND"}
load_fp("Package_SON", "WSON-10-1EP_2.5x2.5mm_P0.5mm_EP1.2x2mm", "U3", "TPS63031DSK", 28, 84, pads=reg_pads)
load_fp("Inductor_SMD", "L_APV_ANR3015", "L1", "1.5uH >=1.5A", 28, 79.5, pads={"1": "REG_L1", "2": "REG_L2"})
passive("C", "C8", "10uF/10V", 23, 84, "SYS", "GND", size="0805")
passive("C", "C9", "100nF", 24, 87, "REG_VINA", "GND")
passive("C", "C10", "10uF/10V", 33, 82.5, "+3V3", "GND", size="0805")
passive("C", "C11", "10uF/10V", 33, 86, "+3V3", "GND", size="0805")


# E-paper FPC on the back. Gold fingers face the display front, so use a
# bottom-contact FH12 footprint and clearly mark pin 1 on both copper sides.
epd_pads = {
    "2": "EPD_GDR", "3": "EPD_RESE", "5": "EPD_VSH2", "6": "GND", "7": "GND",
    "8": "GND", "9": "EPD_BUSY", "10": "EPD_RST", "11": "EPD_DC", "12": "EPD_CS",
    "13": "SPI_SCK", "14": "SPI_MOSI", "15": "+3V3", "16": "+3V3", "17": "GND",
    "18": "EPD_VDD", "20": "EPD_VSH1", "21": "PREVGH", "22": "EPD_VSL",
    "23": "PREVGL", "24": "EPD_VCOM",
}
load_fp("Connector_FFC-FPC", "Hirose_FH12-24S-0.5SH_1x24-1MP_P0.50mm_Horizontal", "J3", "OSPTEK_4.26IN_24PIN", 30, 69, side="B", pads=epd_pads)

# SSD1677 external boost network from the display manufacturer's reference.
load_fp("Inductor_SMD", "L_APV_ANR3015", "L2", "47uH 500mA", 11, 64, pads={"1": "+3V3", "2": "BOOST_SW"})
load_fp("Package_TO_SOT_SMD", "SOT-23", "Q1", "AO3400A 30V", 17, 64, pads={"1": "EPD_GDR", "2": "EPD_RESE", "3": "BOOST_SW"})
passive("R", "R12", "1M", 15, 59, "EPD_GDR", "GND", rotation=90)
passive("R", "R13", "2.2R 1%", 19, 59, "EPD_RESE", "GND", rotation=90)
load_fp("Diode_SMD", "D_SOD-123", "D1", "MBR0530", 23, 62, pads={"1": "PREVGH", "2": "BOOST_SW"})
load_fp("Diode_SMD", "D_SOD-123", "D2", "MBR0530", 23, 66, pads={"1": "GND", "2": "BOOST_FLY"})
load_fp("Diode_SMD", "D_SOD-123", "D3", "MBR0530", 23, 70, pads={"1": "BOOST_FLY", "2": "PREVGL"})
passive("C", "C12", "4.7uF/25V", 8, 68, "+3V3", "GND", rotation=90, size="0805")
passive("C", "C13", "4.7uF/25V", 19, 68, "BOOST_SW", "BOOST_FLY", rotation=90, size="0805")
passive("C", "C14", "4.7uF/25V", 27, 61, "PREVGH", "GND", rotation=90, size="0805")
passive("C", "C15", "4.7uF/25V", 27, 70, "PREVGL", "GND", rotation=90, size="0805")
passive("C", "C16", "4.7uF/25V", 34, 63, "EPD_VSH2", "GND", rotation=90, size="0805")
passive("C", "C17", "1uF/25V", 37, 63, "+3V3", "GND", rotation=90, size="0805")
passive("C", "C18", "1uF/25V", 40, 63, "EPD_VDD", "GND", rotation=90, size="0805")
passive("C", "C19", "4.7uF/25V", 43, 63, "EPD_VSH1", "GND", rotation=90, size="0805")
passive("C", "C20", "4.7uF/25V", 46, 63, "PREVGH", "GND", rotation=90, size="0805")
passive("C", "C21", "4.7uF/25V", 49, 63, "EPD_VSL", "GND", rotation=90, size="0805")
passive("C", "C22", "4.7uF/25V", 52, 63, "PREVGL", "GND", rotation=90, size="0805")
passive("C", "C23", "1uF/25V", 55, 63, "EPD_VCOM", "GND", rotation=90, size="0805")


# On-board push-pull MicroSD socket in SPI mode. The card inserts from the
# board's right edge. Pads 9/10 are the card-detect switch and are unused.
sd_pads = {
    "1": "SD_DAT2", "2": "SD_CS", "3": "SPI_MOSI", "4": "+3V3",
    "5": "SPI_SCK", "6": "GND", "7": "SD_MISO", "8": "SD_DAT1",
    "SH": "GND",
}
load_fp("Connector_Card", "microSD_HC_Molex_104031-0811", "J4", "Molex 104031-0811 MicroSD", 52, 51, rotation=90, pads=sd_pads)
passive("C", "C25", "100nF", 41, 47, "+3V3", "GND")
passive("C", "C26", "10uF/6.3V", 41, 50, "+3V3", "GND", size="0805")
passive("R", "R21", "10k", 41, 53, "+3V3", "SD_CS")
passive("R", "R22", "10k", 41, 56, "+3V3", "SPI_MOSI")
passive("R", "R23", "10k", 41, 59, "+3V3", "SD_MISO")
passive("R", "R24", "47k", 47, 60, "+3V3", "SD_DAT1")
passive("R", "R25", "47k", 52, 60, "+3V3", "SD_DAT2")


# External five-key harness: BACK, POWER, UP, HOME, DOWN, common GND.
load_fp("Connector_PinHeader_2.54mm", "PinHeader_1x06_P2.54mm_Vertical", "J5", "EXTERNAL_KEYS", 56, 29, pads={"1": "BTN_BACK", "2": "BTN_POWER", "3": "BTN_UP", "4": "BTN_HOME", "5": "BTN_DOWN", "6": "GND"})
button_pullups = (
    ("BTN_BACK", 42, 38), ("BTN_POWER", 46, 38), ("BTN_UP", 50, 38),
    ("BTN_HOME", 46, 42), ("BTN_DOWN", 50, 42),
)
for idx, (name, x, y) in enumerate(button_pullups, start=14):
    passive("R", f"R{idx}", "10k", x, y, "+3V3", name)


# Battery measurement: 1M / 330k divider gives 1.04 V at a 4.2 V cell.
passive("R", "R19", "1M", 7, 73, "VBAT", "BAT_ADC")
passive("R", "R20", "330k", 12, 73, "BAT_ADC", "GND")
passive("C", "C24", "100nF", 16, 73, "BAT_ADC", "GND")


# Test points for first-board bring-up, particularly the high-voltage EPD rails.
test_points = (
    ("GND", 8, 55), ("+3V3", 14.3, 55), ("VBAT", 20.6, 55),
    ("SYS", 26.9, 55), ("EPD_GDR", 33.2, 55), ("EPD_RESE", 37.5, 55),
    ("PREVGH", 47, 73), ("PREVGL", 54, 73),
)
for idx, (signal, x, y) in enumerate(test_points, start=1):
    load_fp("TestPoint", "TestPoint_Pad_D1.0mm", f"TP{idx}", signal, x, y, pads={"1": signal})


# Add orientation and safety notes to silkscreen.
def text_item(text, x, y, layer=pcbnew.F_SilkS, size=1.0, thickness=0.16, angle=0):
    item = pcbnew.PCB_TEXT(board)
    item.SetText(text)
    item.SetPosition(point(x, y))
    item.SetLayer(layer)
    if layer == pcbnew.B_SilkS:
        item.SetMirrored(True)
    item.SetTextSize(point(size, size))
    item.SetTextThickness(mm(thickness))
    item.SetTextAngleDegrees(angle)
    board.Add(item)


text_item("AI MOBILE READER REV A", 30, 4, size=1.2)
text_item("ESP32-S3-WROOM-1-N16R8", 30, 39, size=0.9)
text_item("PIN 1", 39, 66, layer=pcbnew.B_SilkS, size=0.85)
text_item("GOLD CONTACTS FACE DISPLAY FRONT", 30, 75, layer=pcbnew.B_SilkS, size=0.85)
text_item("1S PROTECTED LIPO", 12, 76, size=0.8)
text_item("USB-C", 30, 88, size=0.8)


# Antenna copper keepout on all copper layers, following Espressif's module rule.
zone = pcbnew.ZONE(board)
zone.SetIsRuleArea(True)
zone.SetDoNotAllowTracks(True)
zone.SetDoNotAllowVias(True)
zone.SetDoNotAllowZoneFills(True)
zone.SetLayerSet(pcbnew.LSET.AllCuMask(board.GetCopperLayerCount()))
outline = pcbnew.SHAPE_LINE_CHAIN()
for xy in ((19, 1.2), (41, 1.2), (41, 14.5), (19, 14.5)):
    outline.Append(point(*xy))
outline.SetClosed(True)
zone.AddPolygon(outline)
board.Add(zone)


# Solid ground reference planes on both inner layers.  The module's rule area
# above keeps both planes out of the antenna region.
for layer in (pcbnew.In1_Cu, pcbnew.In2_Cu):
    plane = pcbnew.ZONE(board)
    plane.SetLayer(layer)
    plane.SetNet(net("GND"))
    plane.SetLocalClearance(mm(0.25))
    for xy in ((1.5, 1.5), (58.5, 1.5), (58.5, 102.5), (1.5, 102.5)):
        plane.AppendCorner(point(*xy), -1)
    board.Add(plane)


# Conservative net classes: USB differential pair, power and EPD high-voltage rails.
settings = board.GetDesignSettings()
default_nc = settings.m_NetSettings.GetDefaultNetclass()
default_nc.SetClearance(mm(0.20))
default_nc.SetTrackWidth(mm(0.20))
default_nc.SetViaDiameter(mm(0.60))
default_nc.SetViaDrill(mm(0.30))
settings.m_TrackMinWidth = mm(0.15)
settings.m_MinClearance = mm(0.15)
settings.m_MinThroughDrill = mm(0.20)
settings.m_HoleClearance = mm(0.15)


# Save the placed, unrouted board. Freerouting imports the DSN and writes tracks.
pcbnew.SaveBoard(str(OUT), board)


# Generate a deterministic BOM directly from the placed board.
rows = []
for fp in sorted(board.GetFootprints(), key=lambda f: f.GetReference()):
    if fp.GetReference().startswith("H") or fp.GetReference().startswith("TP"):
        continue
    rows.append((fp.GetReference(), fp.GetValue(), fp.GetFPID().GetUniStringLibId(), "不装" if fp.GetValue() == "DNP" else "装配"))
with (ROOT / "bom.csv").open("w", newline="", encoding="utf-8") as handle:
    writer = csv.writer(handle)
    writer.writerow(("位号", "参数 / 厂家料号", "封装", "装配状态"))
    writer.writerows(rows)

print(f"Generated {OUT}")
print(f"Footprints: {len(list(board.GetFootprints()))}; nets: {len(NETS)}")
