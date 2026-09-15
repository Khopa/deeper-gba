#!/usr/bin/env python3
"""Turn the monsters' idle sheets (assets/high-res/foes/<foe>-idle.png, a 3 x 3
grid of frames) into the ROM's sprite strips assets/foe_<foe>.png (9 frames of
64 x 64, 15 colours each), through import_anim.py.

usage: import_foes.py

A foe without a sheet of its own borrows another one with its hues shifted
(FALLBACK below), until the drawing exists.
"""
import os
import subprocess
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "..", "assets")
FOES_DIR = os.path.join(ASSETS, "high-res", "foes")
BUILD = os.path.join(HERE, "..", "build")

FOES = ["goblin", "orc", "troll", "demon"]
SHEET = {"goblin": "goblin-idle.png", "orc": "orcs-idle.png", "troll": "troll-idle.png", "demon": "demon-idle.png"}
FALLBACK = {                     # (sheet of another foe, hue shift in degrees, value scale)
    "troll": ("orcs-idle.png", 150, 0.85),
    "demon": ("goblin-idle.png", -25, 0.9),
}


def hue_shift(img, degrees, value):
    """RGBA image with its hue rotated and its brightness scaled."""
    import colorsys
    px = img.load()
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    op = out.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if a < 128:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            h = (h + degrees / 360.0) % 1.0
            r2, g2, b2 = colorsys.hsv_to_rgb(h, s, min(1.0, v * value))
            op[x, y] = (int(r2 * 255), int(g2 * 255), int(b2 * 255), 255)
    return out


def main():
    os.makedirs(BUILD, exist_ok=True)
    for foe in FOES:
        path = os.path.join(FOES_DIR, SHEET[foe])
        if not os.path.exists(path):
            base, deg, val = FALLBACK[foe]
            src = os.path.join(FOES_DIR, base)
            if not os.path.exists(src):
                print(f"{foe}: no sheet ({SHEET[foe]}) and no fallback, skipped")
                continue
            path = os.path.join(BUILD, f"foe_{foe}_fallback.png")
            hue_shift(Image.open(src).convert("RGBA"), deg, val).save(path)
            print(f"{foe}: no {SHEET[foe]} yet, borrowing {base} with the hues shifted")
        out = os.path.join(ASSETS, f"foe_{foe}.png")
        subprocess.check_call([sys.executable, os.path.join(HERE, "import_anim.py"), path,
                               "--grid", "3x3", "--size", "64", "--out", out])
    return 0


if __name__ == "__main__":
    sys.exit(main())
