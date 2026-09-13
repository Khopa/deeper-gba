#!/usr/bin/env python3
"""Record docs/fullrun.gif: play the full-run scenario (tests/emu/scenarios/
07_full_run.lua, fixed seed) in mGBA unthrottled, grabbing a screenshot every
few frames, then assemble an animated GIF.

usage: make_fullrun.py [--rom build/deeper.gba] [--mgba PATH] [--every 5]
                       [--fps 20] [--scale 1] [--out docs/fullrun.gif]
Reuses tests/emu/run.py (symbols, offsets, lib.lua), so the recording is the
very same input sequence as the test.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "tests", "emu"))
import run as emu  # noqa: E402

from PIL import Image  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--rom", default=os.path.join(ROOT, "build", "deeper.gba"))
    ap.add_argument("--mgba", default=next((p for p in emu.MGBA_CANDIDATES if p and os.path.exists(p)), None))
    ap.add_argument("--every", type=int, default=5, help="capture one frame out of N (60 fps source)")
    ap.add_argument("--fps", type=int, default=20, help="playback rate of the GIF")
    ap.add_argument("--scale", type=int, default=1)
    ap.add_argument("--scenario", default="07_full_run")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "fullrun.gif"))
    a = ap.parse_args()
    if not a.mgba:
        sys.exit("mGBA not found: pass --mgba or set MGBA")

    work = os.path.join(ROOT, "build", "fullrun")
    frames_dir = os.path.join(work, "frames")
    shutil.rmtree(frames_dir, ignore_errors=True)
    os.makedirs(frames_dir, exist_ok=True)
    rom = os.path.join(work, "rom.gba")
    shutil.copyfile(a.rom, rom)
    for f in ("rom.sav",):
        if os.path.exists(os.path.join(work, f)):
            os.remove(os.path.join(work, f))

    syms = emu.symbol_addresses(os.path.splitext(a.rom)[0] + ".elf")
    offs = emu.struct_offsets()
    with open(os.path.join(emu.SCENARIOS, a.scenario + ".lua"), encoding="utf-8") as f:
        source = f.read()
    cfg_extra = f'CFG.frames_dir = "{frames_dir.replace(chr(92), "/")}"; CFG.every = {a.every}\n'
    emu.WORK = work
    script = emu.build_script(a.scenario, cfg_extra + source, syms, offs, work.replace("\\", "/"))
    subprocess.run([a.mgba, "-C", "videoSync=0", "-C", "audioSync=0", "--script", script, rom],
                   timeout=900, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    files = sorted(glob.glob(os.path.join(frames_dir, "*.png")))
    if not files:
        sys.exit(f"no frames captured (see {work}/{a.scenario}.log)")
    frames = []
    for path in files:
        im = Image.open(path).convert("RGB")
        if a.scale != 1:
            im = im.resize((im.width * a.scale, im.height * a.scale), Image.NEAREST)
        frames.append(im.quantize(colors=64, method=Image.Quantize.MEDIANCUT))
    duration = int(1000 / a.fps)
    frames[0].save(a.out, save_all=True, append_images=frames[1:], duration=duration, loop=0, optimize=True)
    size = os.path.getsize(a.out) / 1e6
    print(f"{a.out}: {len(frames)} frames, {len(frames) / a.fps:.0f} s at {a.fps} fps, {size:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
