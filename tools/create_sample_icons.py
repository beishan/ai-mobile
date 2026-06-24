#!/usr/bin/env python3
"""Create sample 64x64 icon PNGs for testing the bitmap icon pipeline.

These icons visually resemble the existing procedural icons in icons.c.
Run this script, then run generate_icons.py, then rebuild the simulator.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ICON_DIR = Path("assets/icons")
SIZE = 64
BG = (255, 255, 255)  # white background
FG = (0, 0, 0)  # black foreground


def create_reader() -> Image.Image:
    """Open book with two pages and a bookmark."""
    img = Image.new("RGB", (SIZE, SIZE), BG)
    d = ImageDraw.Draw(img)
    # Left page
    d.rectangle([8, 16, 30, 54], outline=FG, width=2)
    # Right page
    d.rectangle([34, 16, 56, 54], outline=FG, width=2)
    # Spine
    d.rectangle([30, 16, 34, 54], fill=FG)
    # Left page text lines
    d.rectangle([12, 22, 26, 24], fill=FG)
    d.rectangle([12, 30, 26, 32], fill=FG)
    d.rectangle([12, 38, 22, 40], fill=FG)
    # Right page text lines
    d.rectangle([38, 22, 52, 24], fill=FG)
    d.rectangle([38, 30, 52, 32], fill=FG)
    # Bookmark
    d.rectangle([44, 16, 50, 32], fill=FG)
    return img


def create_weather() -> Image.Image:
    """Sun with rays and a cloud with rain."""
    img = Image.new("RGB", (SIZE, SIZE), BG)
    d = ImageDraw.Draw(img)
    # Sun body
    d.ellipse([14, 10, 34, 30], outline=FG, width=2)
    # Sun rays
    d.line([24, 4, 24, 10], fill=FG, width=2)
    d.line([24, 30, 24, 36], fill=FG, width=2)
    d.line([8, 20, 14, 20], fill=FG, width=2)
    d.line([34, 20, 40, 20], fill=FG, width=2)
    d.line([11, 11, 14, 14], fill=FG, width=2)
    d.line([34, 14, 37, 11], fill=FG, width=2)
    d.line([11, 29, 14, 26], fill=FG, width=2)
    d.line([34, 26, 37, 29], fill=FG, width=2)
    # Cloud body
    d.ellipse([22, 32, 56, 48], outline=FG, width=2)
    d.ellipse([16, 36, 38, 52], outline=FG, width=2)
    d.ellipse([30, 28, 50, 44], outline=FG, width=2)
    # Rain drops
    d.line([26, 52, 24, 58], fill=FG, width=2)
    d.line([34, 52, 32, 58], fill=FG, width=2)
    d.line([42, 52, 40, 58], fill=FG, width=2)
    return img


def create_calendar() -> Image.Image:
    """Calendar page with pins and date grid."""
    img = Image.new("RGB", (SIZE, SIZE), BG)
    d = ImageDraw.Draw(img)
    # Calendar body
    d.rectangle([10, 10, 54, 58], outline=FG, width=2)
    # Header separator
    d.line([10, 22, 54, 22], fill=FG, width=2)
    # Pins
    d.rectangle([20, 4, 24, 18], fill=FG)
    d.rectangle([44, 4, 48, 18], fill=FG)
    # Date grid dots
    for r in range(3):
        for c in range(3):
            x = 18 + c * 14
            y = 28 + r * 10
            d.rectangle([x, y, x + 6, y + 6], fill=FG)
    # Large date
    d.rectangle([26, 42, 46, 56], fill=FG)
    return img


def create_english() -> Image.Image:
    """Document with 'Aa' letters and a folded corner."""
    img = Image.new("RGB", (SIZE, SIZE), BG)
    d = ImageDraw.Draw(img)
    # Document body
    d.rectangle([10, 10, 54, 54], outline=FG, width=2)
    # Corner fold
    d.polygon([(42, 10), (54, 10), (54, 22)], fill=FG)
    # Text line
    d.rectangle([14, 42, 50, 44], fill=FG)
    # Letter A
    d.line([22, 18, 16, 40], fill=FG, width=2)
    d.line([16, 40, 28, 40], fill=FG, width=2)
    d.line([28, 40, 22, 18], fill=FG, width=2)
    d.line([19, 32, 25, 32], fill=FG, width=2)
    # Letter a (smaller)
    d.ellipse([30, 28, 50, 42], outline=FG, width=2)
    d.line([30, 35, 40, 35], fill=FG, width=2)
    d.line([30, 35, 30, 42], fill=FG, width=2)
    return img


def create_settings() -> Image.Image:
    """Three horizontal slider bars with toggles."""
    img = Image.new("RGB", (SIZE, SIZE), BG)
    d = ImageDraw.Draw(img)
    # Three slider tracks
    for i, y in enumerate([18, 32, 46]):
        d.rectangle([12, y, 52, y + 4], outline=FG, width=1)
        # Toggle at different positions
        tx = [36, 18, 28][i]
        d.rectangle([tx, y - 2, tx + 12, y + 6], outline=FG, width=2)
        # Toggle handle
        d.rectangle([tx + 3, y + 1, tx + 9, y + 3], fill=FG)
    return img


def create_about() -> Image.Image:
    """Info panel with 'i' symbol."""
    img = Image.new("RGB", (SIZE, SIZE), BG)
    d = ImageDraw.Draw(img)
    # Panel
    d.rectangle([14, 10, 50, 54], outline=FG, width=2)
    # Dot
    d.ellipse([28, 18, 36, 26], fill=FG)
    # Bar
    d.rectangle([28, 30, 36, 46], fill=FG)
    # Underline
    d.rectangle([22, 50, 42, 52], fill=FG)
    return img


GENERATORS = {
    "reader": create_reader,
    "weather": create_weather,
    "calendar": create_calendar,
    "english": create_english,
    "settings": create_settings,
    "about": create_about,
}


def main() -> None:
    ICON_DIR.mkdir(parents=True, exist_ok=True)
    for name, gen in GENERATORS.items():
        img = gen()
        path = ICON_DIR / f"{name}.png"
        img.save(path)
        print(f"  Created {path} ({SIZE}x{SIZE})")
    print(f"\nDone. Now run: python3 tools/generate_icons.py")


if __name__ == "__main__":
    main()
