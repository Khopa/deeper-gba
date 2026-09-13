#!/usr/bin/env python3
"""Make the menu logo (assets/logo.png, a tiled picture) from a drawing.

usage: import_logo.py assets/high-res/title-text.png

The drawing is cropped to its opaque bounds (rounded out to whole 8 px tiles,
at most 240 x 96), quantised to 15 colours picked from its own colours and
centred in a 256 x 128 sheet: eight 64 x 64 sprite pieces (--meta 8 8) the
menu scales together with one affine matrix.
"""
import argparse
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from import_anim import farthest_palette, color_dist  # noqa: E402
from import_concept import MAGENTA  # noqa: E402

ASSETS = os.path.join(HERE, "..", "assets")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source")
    a = ap.parse_args()
    img = Image.open(a.source).convert("RGBA")
    x0, y0, x1, y1 = img.getbbox()
    x0, y0 = x0 - x0 % 8, y0 - y0 % 8
    x1, y1 = min(x0 + 240, (x1 + 7) // 8 * 8), min(y0 + 96, (y1 + 7) // 8 * 8)
    img = img.crop((x0, y0, x1, y1))
    counts = {}
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, al = px[x, y]
            if al >= 128:
                counts[(r, g, b)] = counts.get((r, g, b), 0) + 1
    palette = farthest_palette(counts, 15)
    box = (256, 128)                                  # 4 x 2 sprite pieces of 64 px
    off = ((box[0] - img.width) // 2, (box[1] - img.height) // 2)
    out = Image.new("P", box, 0)
    op = out.load()
    cache = {}
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, al = px[x, y]
            if al < 128:
                continue
            key = (r, g, b)
            if key not in cache:
                cache[key] = 1 + min(range(len(palette)), key=lambda i: color_dist(key, palette[i]))
            op[off[0] + x, off[1] + y] = cache[key]
    flat = list(MAGENTA)
    for c in palette:
        flat += list(c)
    flat += [0, 0, 0] * (256 - 1 - len(palette))
    out.putpalette(flat)
    out.save(os.path.join(ASSETS, "logo.png"))
    with open(os.path.join(ASSETS, "logo.opts"), "w", newline="\n") as f:
        f.write("--meta 8 8\n")
    print(f"logo.png {img.width}x{img.height} from {os.path.basename(a.source)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
