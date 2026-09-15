#!/usr/bin/env python3
"""Cut the game's PNG assets out of a concept sheet (one big mock-up image).

usage: import_concept.py sheet.png [--out assets] [--preview build/concept_preview.png]

Every crop box below is a region of the sheet; it is scaled to the asset's
size, keyed (the flat panel background flood-filled from the crop border
becomes transparent) and quantised to the palette budget of its slot:
sprites and marks to 15 colours + transparency. The title picture, the menu
buttons, the merchant, the biome backdrops and the small icons come from
their own drawings (tools/import_title.py, import_anim.py, import_tiles.py,
import_icons.py).
Re-run after editing the boxes; make_assets.py keeps drawing the assets this
script does not cover.
"""
import argparse
import os
import sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_assets as ma  # noqa: E402  (font glyphs, opts writer, palettes)

MAGENTA = (255, 0, 255)


# --- helpers ---------------------------------------------------------------------------

def key_background(img, threshold=48):
    """Flood-fill from the border: pixels close to the border colour become transparent."""
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    border = []
    for x in range(w):
        border += [px[x, 0][:3], px[x, h - 1][:3]]
    for y in range(h):
        border += [px[0, y][:3], px[w - 1, y][:3]]
    avg = tuple(sum(c[i] for c in border) // len(border) for i in range(3))

    def close(c):
        return sum((c[i] - avg[i]) ** 2 for i in range(3)) ** 0.5 < threshold

    seen = bytearray(w * h)
    stack = [(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)]
    while stack:
        x, y = stack.pop()
        if x < 0 or y < 0 or x >= w or y >= h or seen[y * w + x]:
            continue
        seen[y * w + x] = 1
        if not close(px[x, y][:3]):
            continue
        px[x, y] = (0, 0, 0, 0)
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return img


def to_indexed(img, colors, first_index, transparent=True):
    """RGBA -> indexed PNG using indices first_index.. (0 = transparent when transparent)."""
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    rgb = Image.new("RGB", (w, h))
    rp = rgb.load()
    opaque = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            rp[x, y] = (r, g, b) if (a >= 128 or not transparent) else (0, 0, 0)
            if a >= 128 or not transparent:
                opaque.append((x, y))
    q = rgb.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    qpal = q.getpalette()[: colors * 3]
    qp = q.load()
    out = Image.new("P", (w, h), 0)
    op = out.load()
    for (x, y) in opaque:
        op[x, y] = qp[x, y] + first_index
    pal = [0, 0, 0] * 256
    pal[0:3] = list(MAGENTA)
    for i in range(colors):
        pal[(first_index + i) * 3:(first_index + i) * 3 + 3] = qpal[i * 3:i * 3 + 3]
    out.putpalette(pal)
    return out


def to_indexed_farthest(img, colors, first_index):
    """Opaque RGB image with few distinct colours -> indexed, keeping the rare
    hues: colours are picked farthest-first (weighted by use), then every
    pixel maps to its nearest pick. Median cut would merge a few purple
    crystals into the grey rock around them."""
    img = img.convert("RGB")
    w, h = img.size
    counts = {}
    for c in img.getdata():
        counts[c] = counts.get(c, 0) + 1
    src = sorted(counts, key=lambda c: -counts[c])

    def dist(a, b):
        return (a[0] - b[0]) ** 2 * 2 + (a[1] - b[1]) ** 2 * 3 + (a[2] - b[2]) ** 2

    chosen = [src[0]]
    while len(chosen) < min(colors, len(src)):
        best, best_score = None, -1
        for c in src:
            if c in chosen:
                continue
            score = min(dist(c, k) for k in chosen) * (counts[c] ** 0.25)
            if score > best_score:
                best, best_score = c, score
        chosen.append(best)
    nearest = {c: min(range(len(chosen)), key=lambda i: dist(c, chosen[i])) for c in src}
    out = Image.new("P", (w, h), 0)
    op, px = out.load(), img.load()
    for y in range(h):
        for x in range(w):
            op[x, y] = nearest[px[x, y]] + first_index
    pal = [0, 0, 0] * 256
    pal[0:3] = list(MAGENTA)
    for i, c in enumerate(chosen):
        pal[(first_index + i) * 3:(first_index + i) * 3 + 3] = list(c)
    out.putpalette(pal)
    return out


def to_indexed_farthest_rgba(img, colors):
    """RGBA -> indexed: transparent pixels to index 0, the opaque ones to the
    nearest of `colors` picks made farthest-first among the drawing's colours."""
    w, h = img.size
    px = img.load()
    counts = {}
    for y in range(h):
        for x in range(w):
            r, g, b, al = px[x, y]
            if al >= 128:
                counts[(r, g, b)] = counts.get((r, g, b), 0) + 1
    src = sorted(counts, key=lambda c: -counts[c])

    def dist(a, b):
        return (a[0] - b[0]) ** 2 * 2 + (a[1] - b[1]) ** 2 * 3 + (a[2] - b[2]) ** 2

    chosen = [src[0]]
    while len(chosen) < min(colors, len(src)):
        best, best_score = None, -1
        for c in src:
            if c in chosen:
                continue
            score = min(dist(c, k) for k in chosen) * (counts[c] ** 0.25)
            if score > best_score:
                best, best_score = c, score
        chosen.append(best)
    nearest = {c: 1 + min(range(len(chosen)), key=lambda i: dist(c, chosen[i])) for c in src}
    out = Image.new("P", (w, h), 0)
    op = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b, al = px[x, y]
            if al >= 128:
                op[x, y] = nearest[(r, g, b)]
    pal = list(MAGENTA)
    for c in chosen:
        pal += list(c)
    pal += [0, 0, 0] * (256 - 1 - len(chosen))
    out.putpalette(pal)
    return out


def crop_scaled(sheet, box, size, keyed=True, threshold=48, pad=0):
    img = sheet.crop(box)
    if keyed:
        img = key_background(img, threshold)
    else:
        img = img.convert("RGBA")
    # fit inside `size` keeping the aspect ratio, centred, with optional padding
    tw, th = size[0] - 2 * pad, size[1] - 2 * pad
    scale = min(tw / img.width, th / img.height)
    nw, nh = max(1, round(img.width * scale)), max(1, round(img.height * scale))
    img = img.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.paste(img, ((size[0] - nw) // 2, (size[1] - nh) // 2), img)
    return canvas


def strip(frames, size, colors=15, first_index=1):
    """Horizontal strip of same-size RGBA frames -> one indexed image (shared palette)."""
    w, h = size
    sheet = Image.new("RGBA", (w * len(frames), h), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        sheet.paste(f, (i * w, 0), f)
    return to_indexed(sheet, colors, first_index)


def tileable(img, margin):
    """Make a square crop repeat without seams: the image rolled by half a period
    has the old edges meeting in the middle; that cross is covered by a blend
    with the original image, which is continuous there."""
    w, h = img.size
    rolled = Image.new("RGB", (w, h))
    rolled.paste(img.crop((w // 2, h // 2, w, h)), (0, 0))
    rolled.paste(img.crop((0, h // 2, w // 2, h)), (w // 2, 0))
    rolled.paste(img.crop((w // 2, 0, w, h // 2)), (0, h // 2))
    rolled.paste(img.crop((0, 0, w // 2, h // 2)), (w // 2, h // 2))
    out = Image.new("RGB", (w, h))
    op, ip, rp = out.load(), img.load(), rolled.load()
    for y in range(h):
        wy = max(0.0, 1.0 - abs(y + 0.5 - h / 2) / margin)
        for x in range(w):
            wx = max(0.0, 1.0 - abs(x + 0.5 - w / 2) / margin)
            k = max(wx, wy)
            op[x, y] = tuple(int(ip[x, y][i] * k + rp[x, y][i] * (1 - k)) for i in range(3))
    return out


def blit_text(img, text, x, y, color):
    """Game font (make_assets glyphs), 1x, onto an RGBA image."""
    px = img.load()
    for k, ch in enumerate(text):
        if ch == " ":
            continue
        for yy, row in enumerate(ma.glyph_rows(ch)):
            for xx, c in enumerate(row):
                if c == "#":
                    px[x + k * 7 + xx, y + yy] = color


# --- the sheet map ------------------------------------------------------------------------
# (x0, y0, x1, y1) boxes on the 1536 x 1024 sheet

BUTTONS = {                                          # GBA keys
    "A": (342, 644, 388, 690), "B": (342, 698, 388, 744),
    "START": (338, 786, 400, 810), "SELECT": (338, 756, 400, 780), "DPAD": (334, 584, 396, 640),
}
DWARF_WALK = (40, 874, 104, 962)
DWARF_DIG = (126, 874, 226, 962)
MARKS = {"dig": (932, 866, 980, 910), "gem": (764, 682, 818, 728), "ore_light": (1024, 682, 1084, 728), "ore_dark": (626, 682, 684, 728)}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("sheet")
    ap.add_argument("--out", default=os.path.join(HERE, "..", "assets"))
    ap.add_argument("--preview", default=os.path.join(HERE, "..", "build", "concept_preview.png"))
    a = ap.parse_args()
    sheet = Image.open(a.sheet).convert("RGB")
    out = a.out
    previews = []

    # GBA buttons: A B L R START SELECT DPAD (L and R stay drawn)
    def drawn_button(name):
        img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        px = img.load()
        colors = {"d": ma.MARK_PAL[1] + (255,), "l": ma.MARK_PAL[2] + (255,), "a": ma.MARK_PAL[3] + (255,), "g": ma.MARK_PAL[4] + (255,), "t": (200, 170, 90, 255)}
        rows = [r.ljust(16, ".") for r in ma.BUTTON_ART[name].strip("\n").split("\n")]
        for y, row in enumerate(rows):
            for x, c in enumerate(row):
                if c != ".":
                    px[x, y] = colors[c]
        return img
    def pill_button(name):
        # the concept pill in the upper half, the drawn caption (rows 11..13) below it
        img = drawn_button(name)
        px = img.load()
        for y in range(11):
            for x in range(16):
                px[x, y] = (0, 0, 0, 0)
        pill = crop_scaled(sheet, BUTTONS[name], (16, 9), pad=1)
        img.paste(pill, (0, 1), pill)
        return img
    frames = [crop_scaled(sheet, BUTTONS["A"], (16, 16)), crop_scaled(sheet, BUTTONS["B"], (16, 16)),
              drawn_button("L"), drawn_button("R"), pill_button("START"), pill_button("SELECT"),
              crop_scaled(sheet, BUTTONS["DPAD"], (16, 16)),
              drawn_button("UP"), drawn_button("DOWN"), drawn_button("LEFT"), drawn_button("RIGHT")]
    strip(frames, (16, 16), colors=14, first_index=2).save(os.path.join(out, "buttons.png"))
    ma.write_opts("buttons.opts", "--meta 2 2")
    previews += [(n, f) for n, f in zip(["A", "B", "L", "R", "START", "SELECT", "DPAD", "UP", "DOWN", "LEFT", "RIGHT"], frames)]

    # player dwarf: idle A/B (walk crop, B bobs), dig A/B (dig crop, B mirrored pick side)
    walk = crop_scaled(sheet, DWARF_WALK, (16, 16))
    dig = crop_scaled(sheet, DWARF_DIG, (16, 16))
    walk_b = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    walk_b.paste(walk.crop((0, 0, 16, 15)), (0, 1), walk.crop((0, 0, 16, 15)))
    dig_b = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    dig_b.paste(dig.crop((0, 1, 16, 16)), (0, 0), dig.crop((0, 1, 16, 16)))
    strip([walk, walk_b, dig, dig_b], (16, 16)).save(os.path.join(out, "dwarf.png"))
    ma.write_opts("dwarf.opts", "--meta 2 2")
    previews += [("dwarf", walk), ("dig", dig)]

    # marks: a whole drawn strip in assets/high-res/marks.png wins outright (24
    # marks of 16 px, magenta or alpha = transparent, quantised to 15 colours)
    strip_path = os.path.join(os.path.dirname(a.sheet), "marks.png")
    if os.path.exists(strip_path):
        drawn = Image.open(strip_path).convert("RGBA")
        px = drawn.load()
        for y in range(drawn.height):
            for x in range(drawn.width):
                r, g, b, al = px[x, y]
                px[x, y] = (0, 0, 0, 0) if (al < 128 or (r, g, b) == MAGENTA) else (r, g, b, 255)
        to_indexed_farthest_rgba(drawn, 15).save(os.path.join(out, "marks.png"))
        previews.append(("marks", drawn.crop((0, 0, 64, 16))))
        marks_done = True
    else:
        marks_done = False
    # otherwise: keep the drawn cross / alert / ghosts / bursts / numbers, replace dig, gem, ores
    marks_img = Image.open(os.path.join(out, "marks.png"))
    if marks_img.mode != "P":
        raise SystemExit("marks.png must be the indexed strip drawn by make_assets.py")
    # a drawing dropped in assets/high-res/marks/<name>.png (any size, transparent
    # background) replaces the concept crop of that mark, or adds one for a
    # mark the sheet does not cover (the names: MARK_ORDER in make_assets.py)
    marks_dir = os.path.join(os.path.dirname(a.sheet), "marks")
    names = list(MARKS)
    frames = []
    for name in names:
        frames.append(crop_scaled(sheet, MARKS[name], (16, 16), pad=1))
    for name in ma.MARK_ORDER:
        path = os.path.join(marks_dir, name + ".png")
        if not os.path.exists(path):
            continue
        img = Image.open(path).convert("RGBA")
        if img.size != (16, 16):
            img = img.resize((16, 16), Image.LANCZOS)
        px = img.load()
        for y in range(16):
            for x in range(16):
                r, g, b, al = px[x, y]
                px[x, y] = (r, g, b, 255) if al >= 128 else (0, 0, 0, 0)
        if name in names:
            frames[names.index(name)] = img
        else:
            names.append(name)
            frames.append(img)
    imported = strip(frames, (16, 16), colors=11, first_index=5)
    mp, ip = marks_img.load(), imported.load()
    for k, name in enumerate(names):
        ox = ma.MARK_ORDER.index(name) * 16
        for y in range(16):
            for x in range(16):
                mp[ox + x, y] = ip[k * 16 + x, y]
    pal = marks_img.getpalette()[:768]
    pal += [0] * (768 - len(pal))
    pal[15:48] = imported.getpalette()[15:48]
    marks_img.putpalette(pal)
    if not marks_done:
        marks_img.save(os.path.join(out, "marks.png"))
    previews += [("mark " + n, f) for n, f in zip(MARKS, frames)]

    # preview sheet
    cell = 72
    cols = 8
    rows = (len(previews) + cols - 1) // cols
    pv = Image.new("RGB", (cols * cell, rows * (cell + 12) + 12), (60, 60, 60))
    d = ImageDraw.Draw(pv)
    for i, (name, img) in enumerate(previews):
        x, y = (i % cols) * cell, (i // cols) * (cell + 12)
        im = img.convert("RGBA")
        scale = max(1, min(64 // im.width, 64 // im.height))
        if im.width > 64:
            im = im.resize((64, int(64 * im.height / im.width)))
        else:
            im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
        pv.paste(im, (x + 4, y + 4), im)
        d.text((x + 4, y + cell - 4), name[:11], fill=(255, 255, 0))
    pv.save(a.preview)
    print("assets written to", os.path.normpath(out), "- preview", os.path.normpath(a.preview))
    return 0


if __name__ == "__main__":
    sys.exit(main())
