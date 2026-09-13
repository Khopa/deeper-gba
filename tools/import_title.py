#!/usr/bin/env python3
"""Make the title picture (assets/title.png, mode 4 bitmap) from a 240 x 160 drawing.

usage: import_title.py assets/high-res/title2.png [--text "PRESS START"] [--y 146]

The drawing is kept pixel for pixel (255 colours at most after the text);
the prompt is baked in with the game font, light on a dark shadow, centred
at the given row. --text "" bakes nothing.
"""
import argparse
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from import_concept import blit_text, to_indexed  # noqa: E402

ASSETS = os.path.join(HERE, "..", "assets")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source")
    ap.add_argument("--text", default="PRESS START")
    ap.add_argument("--y", type=int, default=146)
    a = ap.parse_args()
    img = Image.open(a.source).convert("RGBA")
    if img.size != (240, 160):
        raise SystemExit(f"{a.source}: the title picture must be 240x160 (got {img.width}x{img.height})")
    if a.text:
        x = 120 - len(a.text) * 7 // 2
        for dx, dy in ((1, 1), (1, 0), (0, 1), (-1, 0), (0, -1), (-1, -1), (1, -1), (-1, 1)):
            blit_text(img, a.text, x + dx, a.y + dy, (24, 12, 8, 255))
        blit_text(img, a.text, x, a.y, (255, 236, 180, 255))
    to_indexed(img, 255, 1, transparent=False).save(os.path.join(ASSETS, "title.png"))
    with open(os.path.join(ASSETS, "title.opts"), "w", newline="\n") as f:
        f.write("--bitmap\n")
    print(f"title.png from {os.path.basename(a.source)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
