#!/usr/bin/env python3
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/System/Library/Fonts/PingFang.ttc"
OUT_PATH = Path("assets/fonts/sim_zh16.h")
SIZE = 16

TEXT = """
阅读天气日历游戏英语设置关于书架三体刘慈欣百年孤独马尔克斯活着余华第章黑暗森林
北京晴转多云湿度风力今天明天后天空气质量良更新于农历五月十九
单词学习释义例句认识不认识下一词字体大小行间距省电模式已连接城市固件版本芯片型号
贪吃蛇分上下键移动暂停返回主页上一页下一页刷新中电量不足即将关机目录书签退出
推箱子数独最高分已完成关初级中级高级错误选中当前
年月周一二四五六
"""

ASCII = "".join(chr(i) for i in range(32, 127))
GLYPHS = sorted(set(ASCII + "".join(TEXT.split())))


def glyph_bytes(font: ImageFont.FreeTypeFont, char: str) -> list[int]:
    image = Image.new("L", (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(image)
    bbox = draw.textbbox((0, 0), char, font=font)
    width = bbox[2] - bbox[0]
    height = bbox[3] - bbox[1]
    x = max(0, (SIZE - width) // 2 - bbox[0])
    y = max(0, (SIZE - height) // 2 - bbox[1])
    draw.text((x, y), char, fill=255, font=font)

    data: list[int] = []
    for row in range(SIZE):
        for byte_index in range(SIZE // 8):
            value = 0
            for bit in range(8):
                col = byte_index * 8 + bit
                if image.getpixel((col, row)) > 96:
                    value |= 1 << (7 - bit)
            data.append(value)
    return data


def main() -> None:
    font = ImageFont.truetype(FONT_PATH, SIZE)
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)

    lines: list[str] = []
    lines.append("#ifndef SIM_ZH16_H")
    lines.append("#define SIM_ZH16_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#define SIM_ZH16_WIDTH 16")
    lines.append("#define SIM_ZH16_HEIGHT 16")
    lines.append("#define SIM_ZH16_BYTES_PER_GLYPH 32")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    uint32_t codepoint;")
    lines.append("    uint8_t bitmap[SIM_ZH16_BYTES_PER_GLYPH];")
    lines.append("} sim_zh16_glyph_t;")
    lines.append("")
    lines.append("static const sim_zh16_glyph_t sim_zh16_glyphs[] = {")

    for char in GLYPHS:
        data = glyph_bytes(font, char)
        hex_bytes = ", ".join(f"0x{byte:02x}" for byte in data)
        lines.append(f"    {{0x{ord(char):04x}, {{{hex_bytes}}}}},")

    lines.append("};")
    lines.append("")
    lines.append("static const int sim_zh16_glyph_count = (int)(sizeof(sim_zh16_glyphs) / sizeof(sim_zh16_glyphs[0]));")
    lines.append("")
    lines.append("#endif")
    lines.append("")

    OUT_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT_PATH} with {len(GLYPHS)} glyphs")


if __name__ == "__main__":
    main()
