#!/usr/bin/env python3
"""Draw the placeholder source PNGs used by the game (font, grid cells,
marks, cursor, dwarf sprite).

The pixel art is defined here as ASCII art or drawn with a few PIL
primitives so it is versioned as text and can be tweaked easily. Run once to
(re)generate assets/*.png; png2gba.py turns those PNGs into GBA tile data at
build time. Final hand-drawn art replaces the PNGs with the same names and
layouts (see docs/assets.md) without touching the code.

Every image is written as an indexed (mode "P") PNG whose palette indices are
what the game expects; the real colours are chosen at run time by palette
bank, so the PNG palette is only a preview.
"""
import os
import sys
from PIL import Image

ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")

# Glyph order in font.png; the C side (render.c FONT_CHARS) must match.
# \x01 = enter icon, \x02 = backspace icon, \x03 = bar segment, \x04 = full
# heart, \x05 = empty heart, \x06 = solid 8x8 block
FONT_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?:.-/%><#',\x01\x02\x03\x04\x05\x06"

GLYPHS = {
" ": """
......
......
......
......
......
......
......
""",
"A": """
.####.
#....#
#....#
######
#....#
#....#
#....#
""",
"B": """
#####.
#....#
#....#
#####.
#....#
#....#
#####.
""",
"C": """
.####.
#....#
#.....
#.....
#.....
#....#
.####.
""",
"D": """
#####.
#....#
#....#
#....#
#....#
#....#
#####.
""",
"E": """
######
#.....
#.....
#####.
#.....
#.....
######
""",
"F": """
######
#.....
#.....
#####.
#.....
#.....
#.....
""",
"G": """
.####.
#....#
#.....
#..###
#....#
#....#
.####.
""",
"H": """
#....#
#....#
#....#
######
#....#
#....#
#....#
""",
"I": """
.####.
..##..
..##..
..##..
..##..
..##..
.####.
""",
"J": """
..####
....#.
....#.
....#.
....#.
#...#.
.###..
""",
"K": """
#....#
#...#.
#..#..
###...
#..#..
#...#.
#....#
""",
"L": """
#.....
#.....
#.....
#.....
#.....
#.....
######
""",
"M": """
#....#
##..##
#.##.#
#.##.#
#....#
#....#
#....#
""",
"N": """
#....#
##...#
#.#..#
#..#.#
#...##
#....#
#....#
""",
"O": """
.####.
#....#
#....#
#....#
#....#
#....#
.####.
""",
"P": """
#####.
#....#
#....#
#####.
#.....
#.....
#.....
""",
"Q": """
.####.
#....#
#....#
#....#
#..#.#
#...#.
.###.#
""",
"R": """
#####.
#....#
#....#
#####.
#..#..
#...#.
#....#
""",
"S": """
.#####
#.....
#.....
.####.
.....#
.....#
#####.
""",
"T": """
######
..##..
..##..
..##..
..##..
..##..
..##..
""",
"U": """
#....#
#....#
#....#
#....#
#....#
#....#
.####.
""",
"V": """
#....#
#....#
#....#
#....#
.#..#.
.#..#.
..##..
""",
"W": """
#....#
#....#
#....#
#.##.#
#.##.#
##..##
#....#
""",
"X": """
#....#
#....#
.#..#.
..##..
.#..#.
#....#
#....#
""",
"Y": """
#....#
#....#
.#..#.
..##..
..##..
..##..
..##..
""",
"Z": """
######
.....#
....#.
...#..
..#...
.#....
######
""",
"0": """
.####.
#....#
#...##
#.#..#
##...#
#....#
.####.
""",
"1": """
..##..
.###..
..##..
..##..
..##..
..##..
######
""",
"2": """
.####.
#....#
.....#
...##.
..#...
.#....
######
""",
"3": """
.####.
#....#
.....#
..###.
.....#
#....#
.####.
""",
"4": """
....#.
...##.
..#.#.
.#..#.
######
....#.
....#.
""",
"5": """
######
#.....
#.....
#####.
.....#
#....#
.####.
""",
"6": """
.####.
#.....
#.....
#####.
#....#
#....#
.####.
""",
"7": """
######
.....#
....#.
...#..
..#...
..#...
..#...
""",
"8": """
.####.
#....#
#....#
.####.
#....#
#....#
.####.
""",
"9": """
.####.
#....#
#....#
.#####
.....#
.....#
.####.
""",
"!": """
..##..
..##..
..##..
..##..
..##..
......
..##..
""",
"?": """
.####.
#....#
.....#
...##.
..#...
......
..#...
""",
":": """
......
..##..
..##..
......
..##..
..##..
......
""",
".": """
......
......
......
......
......
..##..
..##..
""",
"-": """
......
......
......
.####.
......
......
......
""",
"/": """
.....#
....#.
...#..
..#...
.#....
#.....
......
""",
"%": """
##...#
##..#.
...#..
..#...
.#....
#..##.
#..##.
""",
">": """
#.....
.#....
..#...
...#..
..#...
.#....
#.....
""",
"<": """
...#..
..#...
.#....
#.....
.#....
..#...
...#..
""",
"#": """
.#..#.
.#..#.
######
.#..#.
######
.#..#.
.#..#.
""",
"'": """
..##..
..##..
...#..
......
......
......
......
""",
",": """
......
......
......
......
..##..
..##..
.#....
""",
# enter icon (return arrow)
"\x01": """
.....#
.....#
..#..#
.#...#
######
.#....
..#...
""",
# backspace icon (left arrow)
"\x02": """
......
..#...
.#....
######
.#....
..#...
......
""",
# bar segment (stat bars); drawn 8 px wide by blit_glyph
"\x03": """
######
######
######
######
######
######
######
""",
# full heart
"\x04": """
.#..#.
######
######
######
.####.
..##..
......
""",
# solid block (drawn by blit_glyph directly)
"\x06": """
######
######
######
######
######
######
######
""",
# empty heart
"\x05": """
.#..#.
#.##.#
#....#
#....#
.#..#.
..##..
......
""",
}

GLYPH_W, GLYPH_H = 6, 7


def glyph_rows(ch):
    rows = [r for r in GLYPHS[ch].strip("\n").split("\n")]
    assert len(rows) == GLYPH_H, f"glyph {ch!r}: {len(rows)} rows"
    for r in rows:
        assert len(r) == GLYPH_W, f"glyph {ch!r}: bad row {r!r}"
    return rows


def blit_glyph(px, ch, x0, y0, color):
    if ch == "\x03":                  # bar segment: full width, rows 1-6
        for y in range(1, 7):
            for x in range(8):
                px[x0 - 1 + x, y0 + y] = color
        return
    if ch == "\x06":                  # solid block: the whole tile
        for y in range(8):
            for x in range(8):
                px[x0 - 1 + x, y0 + y] = color
        return
    for y, row in enumerate(glyph_rows(ch)):
        for x, c in enumerate(row):
            if c == "#":
                px[x0 + x, y0 + y] = color


def new_indexed(w, h, palette):
    img = Image.new("P", (w, h), 0)
    flat = []
    for rgb in palette:
        flat += list(rgb)
    flat += [0, 0, 0] * (256 - len(palette))
    img.putpalette(flat)
    return img


def blit_art(px, art, x0, y0, colors):
    """Paste ASCII art: '.' transparent, other characters looked up in colors."""
    rows = art.strip("\n").split("\n")
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c != ".":
                px[x0 + x, y0 + y] = colors[c]


MAGENTA = (255, 0, 255)


def write_opts(name, text):
    """png2gba options that go with an asset (always LF, the Makefile cats them)."""
    with open(os.path.join(ASSETS, name), "w", newline="\n") as f:
        f.write(text + "\n")


def make_font():
    """8x8 glyph strip, one tile per character of FONT_CHARS (ink = 1)."""
    n = len(FONT_CHARS)
    img = new_indexed(8 * n, 8, [MAGENTA, (255, 255, 255)])
    px = img.load()
    for i, ch in enumerate(FONT_CHARS):
        blit_glyph(px, ch, i * 8 + 1, 0, 1)
    img.save(os.path.join(ASSETS, "font.png"))


# --- grid cells ---------------------------------------------------------------
# 16 metatiles of 16x16: index = thick-edge bits (1 = north, 2 = east,
# 4 = south, 8 = west). Palette: 1 fill, 2 light bevel, 3 dark bevel/grid
# line, 4 region edge. Each rock region gets its own palette bank at run time.
CELL_PAL = [MAGENTA, (150, 120, 90), (185, 155, 120), (95, 72, 50), (30, 22, 16)]


def draw_cell(px, x0, y0, edges):
    for y in range(16):
        for x in range(16):
            c = 1
            if x == 0 or y == 0:
                c = 2
            if x == 15 or y == 15:
                c = 3
            px[x0 + x, y0 + y] = c
    if edges & 1:
        for x in range(16):
            for y in range(2): px[x0 + x, y0 + y] = 4
    if edges & 4:
        for x in range(16):
            for y in range(14, 16): px[x0 + x, y0 + y] = 4
    if edges & 8:
        for y in range(16):
            for x in range(2): px[x0 + x, y0 + y] = 4
    if edges & 2:
        for y in range(16):
            for x in range(14, 16): px[x0 + x, y0 + y] = 4


def make_cells():
    img = new_indexed(16 * 16, 16, CELL_PAL)
    px = img.load()
    for e in range(16):
        draw_cell(px, e * 16, 0, e)
    img.save(os.path.join(ASSETS, "cells.png"))
    write_opts("cells.opts", "--meta 2 2")


# --- marks (overlay on top of the cells) -------------------------------------------
# 16x16 metatiles. Palette: 1 dark ink, 2 light ink, 3 alert, 4 gold.
MARK_PAL = [MAGENTA, (30, 22, 16), (245, 240, 225), (220, 50, 40), (240, 200, 60)]
MARK_ART = {
"cross": """
................
................
................
....d......d....
.....d....d.....
......d..d......
.......dd.......
.......dd.......
......d..d......
.....d....d.....
....d......d....
................
................
................
................
................
""",
"dig": """
................
..........dd....
.........dldd...
........dlldd...
.......dlldddd..
......dlld..dd..
.....dlld....d..
....dlld........
...dlld.........
..dlld..........
..dld...........
..dd............
.dddddd.........
dddddddd........
dddddddd........
.dddddd.........
""",
"alert": """
aaaaaaaaaaaaaaaa
aaaaaaaaaaaaaaaa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aa............aa
aaaaaaaaaaaaaaaa
aaaaaaaaaaaaaaaa
""",
"gem": """
................
................
................
.....gggggg.....
....glllllgg....
...gllggggggg...
...glgggggggg...
...gggggggggg...
....gggggggg....
.....gggggg.....
......gggg......
.......gg.......
................
................
................
................
""",
}
MARK_ORDER = ["empty", "cross", "dig", "alert", "gem"]


def make_marks():
    img = new_indexed(16 * len(MARK_ORDER), 16, MARK_PAL)
    px = img.load()
    colors = {"d": 1, "l": 2, "a": 3, "g": 4}
    for i, name in enumerate(MARK_ORDER):
        if name != "empty":
            blit_art(px, MARK_ART[name], i * 16, 0, colors)
    img.save(os.path.join(ASSETS, "marks.png"))
    write_opts("marks.opts", "--meta 2 2")


# --- cursor sprite (2 frames of 16x16) -----------------------------------------------
CURSOR = """
wwwww......wwwww
w..............w
w..............w
w..............w
w..............w
................
................
................
................
................
................
w..............w
w..............w
w..............w
w..............w
wwwww......wwwww
"""
CURSOR2 = """
wwww........wwww
w..............w
w..............w
w..............w
................
................
................
................
................
................
................
................
w..............w
w..............w
w..............w
wwww........wwww
"""


def make_cursor():
    img = new_indexed(32, 16, [MAGENTA, (255, 255, 255)])
    px = img.load()
    blit_art(px, CURSOR, 0, 0, {"w": 1})
    blit_art(px, CURSOR2, 16, 0, {"w": 1})
    img.save(os.path.join(ASSETS, "cursor.png"))
    write_opts("cursor.opts", "--meta 2 2")


# --- dwarf sprite (16x16, frames: idle A, idle B, dig A, dig B) ------------------------
# Boxy silhouette after Dwarves Manager: square head, wide beard, round helmet.
# Palette: 1 outline, 2 skin, 3 beard, 4 tunic, 5 helmet, 6 pick metal
DWARF_PAL = [MAGENTA, (30, 22, 16), (250, 205, 150), (140, 80, 40), (90, 120, 70), (110, 110, 120), (200, 200, 210)]
DWARF_IDLE_A = """
.....oooooo.....
....ohhhhhho....
...ohhhhhhhho...
...ooooooooo....
...osssssssso...
...osoossooso...
...ossssssso....
...obbbbbbbo....
..obbbbbbbbbo...
..obbbbbbbbbo...
...obbbbbbbo....
...ottttttto....
...ottttttto....
...ottttttto....
...oo.....oo....
...oo.....oo....
"""
DWARF_IDLE_B = """
................
.....oooooo.....
....ohhhhhho....
...ohhhhhhhho...
...ooooooooo....
...osssssssso...
...osoossooso...
...ossssssso....
...obbbbbbbo....
..obbbbbbbbbo...
..obbbbbbbbbo...
...obbbbbbbo....
...ottttttto....
...ottttttto....
...ottttttto....
...oo.....oo....
"""
DWARF_DIG_A = """
.....oooooo...om
....ohhhhhho..om
...ohhhhhhhho.om
...ooooooooo..o.
...osssssssso.o.
...osoossoosoo..
...osssssssoo...
...obbbbbbbo....
..obbbbbbbbbo...
..obbbbbbbbbo...
...obbbbbbbo....
...ottttttto....
...ottttttto....
...ottttttto....
...oo.....oo....
...oo.....oo....
"""
DWARF_DIG_B = """
................
.....oooooo.....
....ohhhhhho....
...ohhhhhhhho...
...ooooooooo....
...osssssssso...
...osoossooso...
...ossssssso....
...obbbbbbbo....
..obbbbbbbbbo...
..obbbbbbbbbooo.
...obbbbbbbo..o.
...ottttttto..o.
...ottttttto.omm
...oo.....oo.omm
...oo.....oo....
"""


def make_dwarf():
    frames = [DWARF_IDLE_A, DWARF_IDLE_B, DWARF_DIG_A, DWARF_DIG_B]
    img = new_indexed(16 * len(frames), 16, DWARF_PAL)
    px = img.load()
    colors = {"o": 1, "s": 2, "b": 3, "t": 4, "h": 5, "m": 6}
    for i, art in enumerate(frames):
        blit_art(px, art, i * 16, 0, colors)
    img.save(os.path.join(ASSETS, "dwarf.png"))
    write_opts("dwarf.opts", "--meta 2 2")


def main():
    os.makedirs(ASSETS, exist_ok=True)
    make_font()
    make_cells()
    make_marks()
    make_cursor()
    make_dwarf()
    print("assets written to", os.path.normpath(ASSETS))


if __name__ == "__main__":
    sys.exit(main())
