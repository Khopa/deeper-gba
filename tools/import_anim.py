#!/usr/bin/env python3
"""Turn an animation sheet (a grid of same-size frames, any resolution) into a
sprite strip the ROM can play.

usage: import_anim.py sheet.png [more.png:COLSxROWS ...] --grid COLSxROWS --size 64 --out assets/merchant.png

Frames are read left to right, top to bottom, scaled to `size` (or centred in
it when smaller, keeping their transparency), quantised together to 15 colours
+ transparent and written as one horizontal strip with the matching .opts
(--meta size/8 size/8). --order 4,3,1,0,2 keeps and reorders frames. Several
sheets (each with its own grid after a colon, --grid otherwise) are chained
into one strip sharing one palette: the dwarf's idle, walk and stills.

    python tools/import_anim.py assets/high-res/merchant-animated.png --grid 3x3 --size 64 --out assets/merchant.png
    python tools/import_anim.py assets/dwarf/dwarf-idle.png assets/dwarf/dwarf-walking.png assets/dwarf/dwarf-48.png:1x1 assets/dwarf/dwarf-back.png:1x1 --grid 3x3 --size 64 --out assets/dwarf.png
"""
import argparse
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from import_concept import MAGENTA  # noqa: E402
import make_assets as ma  # noqa: E402


def color_dist(a, b):
    return (a[0] - b[0]) ** 2 * 2 + (a[1] - b[1]) ** 2 * 3 + (a[2] - b[2]) ** 2


def farthest_palette(counts, colors):
    """Pick `colors` colours out of {colour: pixel count}: the most used first,
    then repeatedly the one farthest from every pick (weighted by use)."""
    src = sorted(counts, key=lambda c: -counts[c])
    chosen = [src[0]]
    while len(chosen) < min(colors, len(src)):
        best, best_score = None, -1
        for c in src:
            if c in chosen:
                continue
            score = min(color_dist(c, k) for k in chosen) * (counts[c] ** 0.25)
            if score > best_score:
                best, best_score = c, score
        chosen.append(best)
    return chosen


def frames_of(sheet, cols, rows):
    fw, fh = sheet.width // cols, sheet.height // rows
    return [sheet.crop((c * fw, r * fh, (c + 1) * fw, (r + 1) * fh)) for r in range(rows) for c in range(cols)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("sheets", nargs="+", help="sheet.png or sheet.png:COLSxROWS")
    ap.add_argument("--grid", required=True, help="COLSxROWS for the sheets without their own, e.g. 3x3")
    ap.add_argument("--size", type=int, required=True, help="frame side in the ROM (16, 32 or 64)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--order", default=None, help="comma-separated frame indices to keep, in output order")
    a = ap.parse_args()
    frames, counts = [], {}
    for spec in a.sheets:
        path, grid = (spec.rsplit(":", 1) if ":" in spec and not spec[-2:].startswith("p") else (spec, a.grid))
        if not os.path.exists(path):                   # a Windows drive letter, not a grid
            path, grid = spec, a.grid
        cols, rows = (int(v) for v in grid.lower().split("x"))
        sheet = Image.open(path).convert("RGBA")
        frames += frames_of(sheet, cols, rows)
        for p in sheet.getdata():
            if p[3] >= 128:
                counts[p[:3]] = counts.get(p[:3], 0) + 1
    if a.order:
        frames = [frames[int(i)] for i in a.order.split(",")]
    size = a.size
    strip = Image.new("RGBA", (size * len(frames), size), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        if f.width < size:                        # smaller art sits centred in the box
            box = Image.new("RGBA", (size, size), (0, 0, 0, 0))
            box.paste(f, ((size - f.width) // 2, (size - f.height) // 2))
            f = box
        elif f.width != size:
            f = f.resize((size, size), Image.LANCZOS)
        # scaling feathers the edge: keep pixels that are mostly opaque
        px = f.load()
        for y in range(size):
            for x in range(size):
                r, g, b, al = px[x, y]
                px[x, y] = (r, g, b, 255) if al >= 128 else (0, 0, 0, 0)
        strip.paste(f, (i * size, 0), f)
    # the palette comes from the source drawing's own colours (farthest-first,
    # weighted by use, so a small gem keeps its blue); the scaled pixels then
    # snap to the nearest of those 15
    palette = farthest_palette(counts, 15)
    out = Image.new("P", strip.size, 0)
    op, sp = out.load(), strip.load()
    cache = {}
    for y in range(strip.height):
        for x in range(strip.width):
            r, g, b, al = sp[x, y]
            if al != 255:
                continue
            key = (r, g, b)
            if key not in cache:
                cache[key] = 1 + min(range(len(palette)), key=lambda i: color_dist(key, palette[i]))
            op[x, y] = cache[key]
    flat = list(MAGENTA)
    for c in palette:
        flat += list(c)
    flat += [0, 0, 0] * (256 - 1 - len(palette))
    out.putpalette(flat)
    out.save(a.out)
    ma.write_opts(os.path.basename(a.out).replace(".png", ".opts"), f"--meta {size // 8} {size // 8}")
    print(f"{len(frames)} frames of {size}x{size} -> {os.path.normpath(a.out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
