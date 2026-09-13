#!/usr/bin/env python3
"""Turn the biome tile sheets (assets/high-res/*.png) into the ROM backdrops.

usage: import_tiles.py [--size N] [--preview build/tiles_preview.png]

A sheet is a horizontal strip of square tiles, 16, 32 or 64 px a side (up to
16 of them). The strip is quantised to the 15 colours of the backdrop palette
bank (indices 1..15; index 1 is the sheet's most used colour) and written as
assets/back_<biome>.png with its .opts; the ROM loads the variants (as many
as fit VRAM) in a random order and paves the screen with them at random.
Blocks that do not divide the 240 px width (32, 64) are centred, whole
columns only, with the side margins painted in that main colour.

Which sheet serves which biome is the SHEETS table below: until every biome
has its own drawing, they all share the crystal cave.
"""
import argparse
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "..", "assets")
HIGHRES = os.path.join(ASSETS, "high-res")
sys.path.insert(0, HERE)
from import_concept import to_indexed, to_indexed_farthest  # noqa: E402
import make_assets as ma  # noqa: E402

BIOMES = ["earth", "rock", "ice", "lava", "crystal", "core"]
DEFAULT_SHEET = "crystal-cave-32.png"
SHEETS = {                      # biome -> sheet file in assets/high-res
    "earth":   DEFAULT_SHEET,
    "rock":    DEFAULT_SHEET,
    "ice":     "icecave.png",
    "lava":    DEFAULT_SHEET,
    "crystal": DEFAULT_SHEET,
    "core":    DEFAULT_SHEET,
}
MAX_VARIANTS = 16               # the ROM keeps up to 16 variants per biome


def sheet_tiles(path):
    img = Image.open(path).convert("RGB")
    side = img.height
    if img.width % side:
        raise SystemExit(f"{path}: a sheet is a strip of square tiles (got {img.width}x{img.height})")
    return [img.crop((i * side, 0, (i + 1) * side, side)) for i in range(img.width // side)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--size", type=int, default=0, help="backdrop tile size in the ROM (default: the sheet's own, 16/32/64)")
    ap.add_argument("--preview", default=os.path.join(HERE, "..", "build", "tiles_preview.png"))
    a = ap.parse_args()
    previews = []
    done = {}
    for biome in BIOMES:
        sheet = SHEETS.get(biome, DEFAULT_SHEET)
        path = os.path.join(HIGHRES, sheet)
        if not os.path.exists(path):
            print(f"{biome}: {sheet} missing, placeholder kept")
            continue
        tiles = sheet_tiles(path)[:MAX_VARIANTS]
        size = a.size or tiles[0].width
        if size not in (16, 32, 64):
            raise SystemExit(f"{sheet}: tiles must be 16, 32 or 64 px (got {size})")
        meta = size // 8
        if sheet not in done:
            strip = Image.new("RGB", (size * len(tiles), size))
            for i, t in enumerate(tiles):
                strip.paste(t.resize((size, size), Image.LANCZOS) if t.width != size else t, (i * size, 0))
            distinct = len(set(strip.getdata()))
            done[sheet] = to_indexed_farthest(strip, 15, 1) if distinct <= 256 else to_indexed(strip, 15, 1, transparent=False)
            previews.append((sheet, strip))
        done[sheet].save(os.path.join(ASSETS, f"back_{biome}.png"))
        ma.write_opts(f"back_{biome}.opts", f"--meta {meta} {meta}")
        print(f"{biome}: {done[sheet].width // size} variants from {sheet}")
    if previews:
        os.makedirs(os.path.dirname(os.path.abspath(a.preview)), exist_ok=True)
        pv = Image.new("RGB", (max(p.width for _, p in previews), sum(p.height + 4 for _, p in previews)), (40, 40, 40))
        y = 0
        for _, p in previews:
            pv.paste(p, (0, y))
            y += p.height + 4
        pv.save(a.preview)
    return 0


if __name__ == "__main__":
    sys.exit(main())
