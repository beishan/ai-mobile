#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
from datetime import datetime
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]

PAGES = {
    "home": "q\n",
    "bookshelf": "h\nq\n",
    "bookshelf_recent": "h\ns\nh\nh\ns\ns\ns\nh\nq\n",
    "bookshelf_bookmark": "h\nh\nh\ns\ns\nh\nh\ns\ns\ns\nh\nq\n",
    "reader": "h\nh\nq\n",
    "reader_font_large": "s\ns\ns\ns\ns\nh\nh\nh\np\nw\nw\nw\nw\nw\nh\nh\nq\n",
    "reader_line_loose": "s\ns\ns\ns\ns\nh\ns\ns\nh\np\nw\nw\nw\nw\nw\nh\nh\nq\n",
    "reader_menu": "h\nh\nh\nq\n",
    "reader_catalog": "h\nh\nh\ns\nh\nq\n",
    "weather": "s\nh\nq\n",
    "weather_city": "s\nh\ns\nq\n",
    "weather_offline": "s\ns\ns\ns\ns\nh\ns\ns\ns\nh\np\nw\nw\nw\nw\nh\nh\nq\n",
    "calendar": "s\ns\nh\nq\n",
    "calendar_next_month": "s\ns\nh\ns\nq\n",
    "calendar_detail": "s\ns\nh\nh\nq\n",
    "game": "s\ns\ns\nh\nq\n",
    "game_snake_running": "s\ns\ns\nh\nh\nh\ns\nq\n",
    "game_snake_over": "s\ns\ns\nh\nh\n" + "h\n" * 32 + "q\n",
    "english": "s\ns\ns\ns\nh\nq\n",
    "english_back": "s\ns\ns\ns\nh\nh\nq\n",
    "english_known": "s\ns\ns\ns\nh\nh\ns\nq\n",
    "english_review": "s\ns\ns\ns\nh\nh\nw\nq\n",
    "settings": "s\ns\ns\ns\ns\nh\nq\n",
    "settings_city": "s\ns\ns\ns\ns\nh\ns\ns\ns\ns\nh\nq\n",
    "settings_wifi_off": "s\ns\ns\ns\ns\nh\ns\ns\ns\nh\nq\n",
    "about": "s\ns\ns\ns\ns\ns\nh\nq\n",
}


def run_capture(page: str, keys: str, out_dir: Path) -> None:
    subprocess.run(
        [str(ROOT / "reader_sim")],
        input=keys,
        text=True,
        cwd=ROOT,
        check=True,
        stdout=subprocess.DEVNULL,
    )
    ppm_path = ROOT / "out" / "frame.ppm"
    png_path = out_dir / f"{page}.png"
    Image.open(ppm_path).save(png_path)


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture timestamped simulator snapshots.")
    parser.add_argument("--label", default="", help="Optional suffix for the snapshot folder.")
    parser.add_argument("--pages", nargs="*", choices=sorted(PAGES), default=sorted(PAGES))
    args = parser.parse_args()

    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    suffix = f"-{args.label}" if args.label else ""
    out_dir = ROOT / "snapshots" / f"{timestamp}{suffix}"
    out_dir.mkdir(parents=True, exist_ok=False)

    subprocess.run(["make", "reader_sim"], cwd=ROOT, check=True)
    for page in args.pages:
        run_capture(page, PAGES[page], out_dir)

    latest = ROOT / "snapshots" / "latest"
    if latest.exists() or latest.is_symlink():
        if latest.is_symlink():
            latest.unlink()
        else:
            shutil.rmtree(latest)
    latest.symlink_to(out_dir.name)

    print(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
