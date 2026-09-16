#!/usr/bin/env python3
"""The game's 16 px icons (assets/nodes/*.png) -> build/gen/gfx_icons.c/.h.

usage: import_icons.py [-o build/gen/gfx_icons]
       import_icons.py --marks [assets/pixelab.png]   (make assets: four marks from the sheet)

Each icon keeps its own palette (quantised to 15 colours when it has more,
the 16th entry being transparent): the ROM loads the palette of the icon it
draws into whichever bank it has free, so nothing is degraded by a shared
palette. A 14 px icon is centred in its 16 px cell. Two tile sets come out:
    iconTiles[ICON_COUNT][32]      colour, 4 tiles (2 x 2, reading order) each
    iconGreyTiles[ICON_COUNT][32]  the same drawn on a 15-step grey ramp
                                   (index = luminance), for the map's far
                                   and passed nodes on one shared bank
    iconPal[ICON_COUNT][16]        BGR555, entry 0 transparent
and the header lists the icons as enum Icon (ICON_<NAME>) in ICONS order.
"""
import argparse
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
NODES = os.path.join(HERE, "..", "assets", "nodes")

# enum Icon order: the file name in assets/nodes/ and what the game uses it for
ICONS = [
    ("mining", "dig rooms"),
    ("ore", "vein rooms; the ore counter"),
    ("stonepile", "block rooms"),
    ("ladder", "tunnel rooms"),
    ("scroll", "ledger rooms"),
    ("coal", "firedamp rooms"),
    ("firecamp", "camps"),
    ("treasure", "the core"),
    ("key", "hint rooms; hint tokens; the merchant's keys"),
    ("potion", "life rooms; the merchant's potion"),
    ("goldore", "risky rooms"),
    ("crate", "the crates"),
    ("sword", "fights"),
    ("wall", "the wall"),
    ("beer", "merchant: maximum lives"),
    ("bread", "merchant: starting lives"),
    ("simplehelmet", "merchant: helmet"),
    ("goldhelmet", "merchant: helmet at its last level"),
    ("boots", "merchant: boots"),
    ("ropes", "merchant: rope"),
    ("amethyst", "vein gem"),
    ("crystal", "vein gem"),
    ("diamond", "vein gem"),
    ("ruby", "vein gem"),
    ("sapphire", "vein gem"),
    ("gold", "spare"),
    ("lantern", "spare"),
    ("torch", "spare"),
    ("minecrat", "spare"),
    ("shovel", "spare"),
]


def rgb15(c):
    r, g, b = c
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


def load_icon(name):
    """16 x 16 RGBA, the icon centred; pixels with alpha < 128 are transparent."""
    img = Image.open(os.path.join(NODES, name + ".png")).convert("RGBA")
    out = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    out.paste(img, ((16 - img.width) // 2, (16 - img.height) // 2))
    px = out.load()
    for y in range(16):
        for x in range(16):
            r, g, b, a = px[x, y]
            px[x, y] = (r, g, b, 255) if a >= 128 else (0, 0, 0, 0)
    return out


def quantise(img, limit=15):
    """Indexed 16 x 16 (0 = transparent) and its palette (RGB tuples)."""
    px0 = img.load()
    opaque = [px0[x, y][:3] for y in range(16) for x in range(16) if px0[x, y][3]]
    colours = []
    for c in opaque:
        if c not in colours:
            colours.append(c)
    if len(colours) > limit:
        strip = Image.new("RGB", (len(opaque), 1))
        strip.putdata(opaque)
        q = strip.quantize(colors=limit, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = q.getpalette()[: 3 * limit]
        colours = [tuple(pal[i:i + 3]) for i in range(0, len(pal), 3)]
    lut = {}

    def nearest(c):
        if c not in lut:
            lut[c] = min(range(len(colours)), key=lambda i: sum((a - b) ** 2 for a, b in zip(colours[i], c)))
        return lut[c]

    idx = [[0] * 16 for _ in range(16)]
    px = img.load()
    for y in range(16):
        for x in range(16):
            if px[x, y][3]:
                idx[y][x] = 1 + nearest(px[x, y][:3])
    return idx, colours


def luminance(c):
    r, g, b = c
    return (299 * r + 587 * g + 114 * b) // 1000


def tiles(idx):
    """4 tiles of 8 u32 (one per row, pixel 0 in the low nibble), 2 x 2 reading order."""
    words = []
    for ty in range(2):
        for tx in range(2):
            for y in range(8):
                w = 0
                for x in range(8):
                    w |= idx[ty * 8 + y][tx * 8 + x] << (4 * x)
                words.append(w)
    return words


# --marks: cells of the Pixel Lab sheet (16 x 16 grid) pasted over the drawn
# marks strip (assets/marks.png, from import_concept.py), then quantised again
MARKS = {
    "dig":       (0, 2),      # small pickaxe
    "gem":       (6, 12),     # light blue diamond
    "ore_light": (2, 15),     # gold nugget
    "ore_dark":  (4, 5),      # coal
}


def import_marks(sheet_path):
    sys.path.insert(0, HERE)
    from import_concept import to_indexed_farthest_rgba, MAGENTA  # noqa: E402
    import make_assets as ma  # noqa: E402
    assets = os.path.join(HERE, "..", "assets")
    sheet = Image.open(sheet_path).convert("RGBA")
    marks = Image.open(os.path.join(assets, "marks.png")).convert("RGBA")
    for name, (r, c) in MARKS.items():
        cell = sheet.crop((c * 16, r * 16, (c + 1) * 16, (r + 1) * 16))
        px = cell.load()
        for y in range(16):
            for x in range(16):
                rr, gg, bb, al = px[x, y]
                px[x, y] = (0, 0, 0, 0) if (al < 128 or (rr, gg, bb) == MAGENTA) else (rr, gg, bb, 255)
        marks.paste(cell, (ma.MARK_ORDER.index(name) * 16, 0))
    to_indexed_farthest_rgba(marks, 15).save(os.path.join(assets, "marks.png"))
    print(f"marks.png: {', '.join(MARKS)} from the sheet")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", default=os.path.join(HERE, "..", "build", "gen", "gfx_icons"))
    ap.add_argument("--marks", nargs="?", const=os.path.join(HERE, "..", "assets", "pixelab.png"), default=None)
    a = ap.parse_args()
    if a.marks:
        import_marks(a.marks)
        return 0
    colour_sets, grey_sets, pals = [], [], []
    for name, _ in ICONS:
        idx, colours = quantise(load_icon(name))
        colour_sets.append(tiles(idx))
        pal = [0] + [rgb15(c) for c in colours]
        pal += [0] * (16 - len(pal))
        pals.append(pal)
        grey = [[0 if v == 0 else 1 + round(luminance(colours[v - 1]) * 14 / 255) for v in row] for row in idx]
        grey_sets.append(tiles(grey))

    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out + ".h", "w", encoding="utf-8", newline="\n") as h:
        h.write("// generated by tools/import_icons.py from assets/nodes/*.png\n#ifndef GFX_ICONS_H\n#define GFX_ICONS_H\n\n")
        h.write("enum Icon {\n")
        for i, (name, use) in enumerate(ICONS):
            h.write(f"    ICON_{name.upper()} = {i},".ljust(28) + f"// {use}\n")
        h.write(f"    ICON_COUNT = {len(ICONS)}\n}};\n\n")
        h.write("extern const unsigned int   iconTiles[ICON_COUNT][32];\n")
        h.write("extern const unsigned int   iconGreyTiles[ICON_COUNT][32];\n")
        h.write("extern const unsigned short iconPal[ICON_COUNT][16];\n\n#endif\n")
    with open(a.out + ".c", "w", encoding="utf-8", newline="\n") as c:
        c.write('// generated by tools/import_icons.py from assets/nodes/*.png\n#include "gfx_icons.h"\n\n')
        for var, sets in (("iconTiles", colour_sets), ("iconGreyTiles", grey_sets)):
            c.write(f"const unsigned int {var}[ICON_COUNT][32] = {{\n")
            for (name, _), words in zip(ICONS, sets):
                c.write(f"    /* {name} */ {{ " + ", ".join(f"0x{w:08x}" for w in words) + " },\n")
            c.write("};\n\n")
        c.write("const unsigned short iconPal[ICON_COUNT][16] = {\n")
        for (name, _), pal in zip(ICONS, pals):
            c.write(f"    /* {name} */ {{ " + ", ".join(f"0x{v:04x}" for v in pal) + " },\n")
        c.write("};\n")
    print(f"{a.out}.c: {len(ICONS)} icons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
