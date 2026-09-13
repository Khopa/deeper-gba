# Deeper — GBA puzzle roguelike (Dwarves Manager spin-off)

Pure C, devkitARM + libtonc (same stack as ../wordle-gba). No C++, no asm.

## Build & test (Windows, MSYS2 devkitPro)
From an MSYS2 shell, or from any shell via
`MSYSTEM=MSYS /c/msys64/usr/bin/bash -lc 'cd /e/Work/dw-deeper && make <target>'`:

    make            build/deeper.gba
    make test       host unit tests (rules, generators, engine logic)
    make emutest    mGBA Lua scenarios (tests/emu); make check = both
    make fullrun    re-record docs/fullrun.gif from the full-run scenario
    make puzzles    regenerate data/puzzles/*.bin with tools/puzzlegen (deterministic seeds)
    make assets     redraw placeholder PNGs (tools/make_assets.py)
    make run        launch in mGBA

## Layout
    common/     platform-independent puzzle rules, shared by the ROM, the PC generator and the tests
    source/     GBA engine (main loop, screens, render, save, run map, per-family play adapters)
    include/    headers for source/ and common/
    tools/puzzlegen/  PC generator+solver+canonicaliser per family -> data/puzzles/*.bin
    tools/      png2gba.py (PNG -> 4bpp tiles), make_assets.py (placeholder art), bin2c.py
    assets/     source PNGs (placeholders now, final art later; same names, no code change)
    data/puzzles/  generated puzzle banks, embedded in ROM
    tests/unit/ host tests (-DHOST_TEST, tests/unit/host_shim.h replaces libtonc)
    tests/emu/  mGBA scenarios (run.py + lib.lua + scenarios/*.lua)
    docs/       design notes (French), formats, art pipeline

## Conventions
- Commit messages: `type(scope): why/what` — types: feat, fix, test, tools, data, build, docs, refactor, art.
  One intention per commit. Scope = family (dig, vein, block, tunnel, ledger, nugget) or subsystem (engine, run, save, render, gen).
- Rules code in common/ must not touch hardware or libc I/O; it is compiled three times (ROM, puzzlegen, tests).
- A feature is done when its unit tests pass (`make test`) and, for engine features, it was checked in mGBA (`make emutest` or a screenshot run).
- Family adapters (source/fam_*.c) never draw: they fill CellView and the room screen renders it, so they stay testable on the host.
- Never name the real-world puzzle origins in player-facing strings.
