#!/usr/bin/env python3
"""Pick the game's small icons out of the Pixel Lab sheet (assets/pixelab.png).

usage: import_icons.py [assets/pixelab.png]

The sheet is a 16 x 16 grid of 16 px icons. The tables below say which cell
serves which icon:
  * NODES   -> assets/nodes.png : the 11 map node icons, the 9 shop goods, the crate
              (one strip, one 15-colour palette; the ROM copies the first 11
              to sprite memory for the rooms' top bar)
  * MARKS   -> assets/marks.png : four marks (dig, gem, light and dark ore)
              pasted over the drawn strip, which is then quantised again
Run after import_concept.py (make assets does).
"""
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "..", "assets")
sys.path.insert(0, HERE)
from import_concept import to_indexed_farthest_rgba, MAGENTA  # noqa: E402
import make_assets as ma  # noqa: E402

# (row, column) on the sheet
NODES = {
    "dig":     (0, 0),    # pickaxe
    "vein":    (1, 10),   # gold ore
    "block":   (3, 6),    # brick wall
    "tunnel":  (7, 7),    # mine entrance
    "ledger":  (10, 7),   # scroll
    "nugget":  (2, 10),   # dark ore with a green glint: firedamp
    "camp":    (3, 1),    # torch
    "core":    (6, 8),    # red gem
    "hint":    (3, 0),    # lantern
    "life":    (14, 8),   # red potion
    "risky":   (2, 11),   # dark ore with a purple eye
}
SHOP = [                  # enum ShopItem order (include/shop.h)
    ("hint",    (10, 8)),     # a golden ring: a hint token
    ("life",    (14, 8)),     # red potion
    ("prop",    (11, 0)),     # a wooden beam
    ("satchel", (9, 3)),      # leather pouch
    ("bedroll", (13, 11)),    # a rolled log/blanket
    ("flask",   (15, 10)),    # blue potion
    ("lantern", (14, 2)),     # hanging lantern
    ("helmet",  (9, 10)),     # golden horned helmet
    ("beard",   (9, 9)),      # the bearded dark helm
]
CRATES = (11, 14)         # a wooden crate: the crates node
FIGHT = (8, 11)           # a horned helm: a monster ahead
MARKS = {
    "dig":       (0, 2),      # small pickaxe
    "gem":       (6, 12),     # light blue diamond
    "ore_light": (2, 15),     # gold nugget
    "ore_dark":  (4, 5),      # coal
}


def cell(sheet, rc):
    r, c = rc
    return sheet.crop((c * 16, r * 16, (c + 1) * 16, (r + 1) * 16))


def clean(img):
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, al = px[x, y]
            px[x, y] = (0, 0, 0, 0) if (al < 128 or (r, g, b) == MAGENTA) else (r, g, b, 255)
    return img


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ASSETS, "pixelab.png")
    sheet = Image.open(path).convert("RGBA")

    names = list(ma.NODE_ORDER) + ["shop_" + n for n, _ in SHOP] + ["crates", "fight"]
    cells = [NODES[n] for n in ma.NODE_ORDER] + [rc for _, rc in SHOP] + [CRATES, FIGHT]
    strip = Image.new("RGBA", (16 * len(cells), 16), (0, 0, 0, 0))
    for i, rc in enumerate(cells):
        strip.paste(cell(sheet, rc), (i * 16, 0))
    to_indexed_farthest_rgba(clean(strip), 15).save(os.path.join(ASSETS, "nodes.png"))
    ma.write_opts("nodes.opts", "--meta 2 2")
    print(f"nodes.png: {len(cells)} icons ({', '.join(names)})")

    marks = Image.open(os.path.join(ASSETS, "marks.png")).convert("RGBA")
    for name, rc in MARKS.items():
        marks.paste(cell(sheet, rc), (ma.MARK_ORDER.index(name) * 16, 0))
    to_indexed_farthest_rgba(clean(marks), 15).save(os.path.join(ASSETS, "marks.png"))
    print(f"marks.png: {', '.join(MARKS)} from the sheet")
    return 0


if __name__ == "__main__":
    sys.exit(main())
