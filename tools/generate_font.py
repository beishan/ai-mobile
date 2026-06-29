#!/usr/bin/env python3
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/System/Library/Fonts/PingFang.ttc"
OUT_DIR = Path("assets/fonts")
SIZES = [12, 14, 16, 18, 20, 22, 24]
MAX_BITMAP_BYTES = 96

TEXT = """
阅读天气日历游戏英语设置关于书架最近三体刘慈欣百年孤独马尔克斯活着余华第章黑暗森林
北京上海广州晴转多云小雨湿度风力今天明天后天空气质量良更新于缓存分钟前农历五月十九
单词学习复习释义例句认识不认识下一词机缘巧合电子阅读器纸张字体宋体大小行间距紧凑标准舒适宽松省电模式已连接城市固件版本芯片型号
贪吃蛇分上下键移动暂停返回主页上一页下一页刷新中电量不足即将关机目录书签退出
推箱子数独最高分已完成关初级中级高级错误选中当前转向前进游戏结束重新开始预览功能
年月周一二三四五六
详情夏至宜阅读无日程提醒
这是一个宁静的夜晚城市灯光在远处闪烁像片低垂星空他翻到等待墨水屏缓慢完成，。
继续查看添加未开关闭项目地址存储卡版本信息
开端转折回声多年以后面对远方吹来热风想起那个潮湿明亮下午小镇钟声很慢从纸页深处传来
田野安静麦穗掠过把书合上又打开仿佛那些旧日子仍然黑白之间缓缓移动
窗外没有月亮只有远处高楼顶端微弱红灯像一颗不肯熄灭信号桌茶已经凉透杯壁上凝着细密水珠
小时候在乡下夏天夜晚铺竹席躺院子里满天星星低得要落来父亲旁边摇蒲扇讲些关于旧事
那时候以为永恒后才知道连光都有走到尽头就像现在坐这间城市屋子
翻本而作者早不在世但文字思想还它们穿过时间黑暗一束固执
资料记录数字坐标那是多年前次观测她记得天天气望远镜那片寂静空
同事们都已经离开只剩个人档案室日光灯嗡嗡响白照纸面把字迹映格外清晰
用手指轻轻按住边角仿佛通过触觉就能触碰年代些背后什么次巧合
还是宇宙深处某文明发出第声呼唤不知道但人类打开了扇门再也关不上
夜班工程师端咖啡走进看眼屏幕又悄悄退出去这种常态今晚似乎深
天线阵列排成弧线倾听耳朵对准方向银河系可疑区域
前曾经收到过段持续了不到秒消失此每天有人守这里等待它再次出现
泛起鱼肚晨轮廓就这样过去觉疲倦
也许因为给种特殊清醒只有在万籁俱寂时才能抵达专注重新拿起笔笔记写几个字
院子里的番石榴树结果了孩子们光着脚跑来跑去笑声被阳光晒过铃铛坐在走廊藤椅
手里捧翻旧书页卷起风从吹带泥土花草气味
闭眼睛听见河水声音条河边流多少年紧慢流着
对岸山丘上有很高很大据说建立之前就存在比任何记忆都要久远
广场鸽子扑棱翅膀飞起来空中绕圈落原处卖冰老头推车走过
车轮碾石板路混起变成慵懒节奏廊柱下打盹帽歪到
走得很慢慢几乎可以忽略计确在看动
雨滴打铁皮屋顶发出密密麻麻声响积水没过脚踝却怕踩
站窗前着切想起前也有那样日候年轻穿白色裙站同
雨季总让太多西已经远去和事声中重新变得清晰洗画
埂草长高漫远处烧秸秆淡烟味顺飘过来候
秋天到处烟空焦甜味道跟身后走块田
背影座宽阔沉默说跟孩子说话看着
那些年路村县换茬渐弯
牛步尾巴甩赶苍蝇惯拐停
跟着用想方向安心些日子多只需步前
太阳顶移短又沟渠浅偶尔看见鱼年轻时也总想赶到前急了反看见更
东两临会你傍具净化升及口叶名哪墙好孔定实家层屉屑干并形往快抽摊放族昏显木柔棵楚法洁炊物状生留痕盒盖稳答股贴跳软避锁问降难题黄？
"""

ASCII = "".join(chr(i) for i in range(32, 127))
GLYPHS = sorted(set(ASCII + "".join(TEXT.split())))


def glyph_record(font: ImageFont.FreeTypeFont, char: str, size: int) -> tuple[int, int, int, list[int]]:
    image = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(image)
    bbox = draw.textbbox((0, 0), char, font=font)
    width = max(1, min(size, bbox[2] - bbox[0]))
    height = max(1, min(size, bbox[3] - bbox[1]))

    def clamp_position(centered: int, lower: int, upper: int) -> int:
        if upper < lower:
            return lower
        return max(lower, min(upper, centered))

    x = clamp_position((size - width) // 2 - bbox[0], -bbox[0], size - bbox[2])
    y = clamp_position((size - height) // 2 - bbox[1], -bbox[1], size - bbox[3])
    draw.text((x, y), char, fill=255, font=font)

    advance = int(round(draw.textlength(char, font=font)))
    if ord(char) < 128:
        advance = max(3, min(size, advance))
    else:
        advance = size

    bytes_per_row = (size + 7) // 8
    data: list[int] = []
    for row in range(size):
        for byte_index in range(bytes_per_row):
            value = 0
            for bit in range(8):
                col = byte_index * 8 + bit
                if col < size and image.getpixel((col, row)) > 96:
                    value |= 1 << (7 - bit)
            data.append(value)
    return width, height, advance, data


def write_font(size: int) -> None:
    font = ImageFont.truetype(FONT_PATH, size)
    ident = f"sim_zh{size}"
    guard = f"SIM_ZH{size}_H"
    out_path = OUT_DIR / f"{ident}.h"
    bytes_per_row = (size + 7) // 8
    bytes_per_glyph = bytes_per_row * size

    lines: list[str] = []
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define SIM_ZH{size}_SIZE {size}")
    lines.append(f"#define SIM_ZH{size}_BYTES_PER_GLYPH {bytes_per_glyph}")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    uint32_t codepoint;")
    lines.append("    uint8_t width;")
    lines.append("    uint8_t height;")
    lines.append("    uint8_t advance;")
    lines.append(f"    uint8_t bitmap[{MAX_BITMAP_BYTES}];")
    lines.append(f"}} {ident}_glyph_t;")
    lines.append("")
    lines.append(f"static const {ident}_glyph_t {ident}_glyphs[] = {{")

    for char in GLYPHS:
        width, height, advance, data = glyph_record(font, char, size)
        data = data + [0] * (MAX_BITMAP_BYTES - len(data))
        hex_bytes = ", ".join(f"0x{byte:02x}" for byte in data)
        lines.append(f"    {{0x{ord(char):04x}, {width}, {height}, {advance}, {{{hex_bytes}}}}},")

    lines.append("};")
    lines.append("")
    lines.append(f"static const int {ident}_glyph_count = (int)(sizeof({ident}_glyphs) / sizeof({ident}_glyphs[0]));")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out_path} with {len(GLYPHS)} glyphs")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for size in SIZES:
        write_font(size)


if __name__ == "__main__":
    main()
