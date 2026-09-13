# Deeper

A puzzle roguelike for the Game Boy Advance, spin-off of *Dwarves Manager*
(Android, 2013). Dwarves dig their way down through the underground, run after
run: about thirty rooms of logic puzzles on a branching map, from the surface
to the core of the earth.

*Version française : [README.fr.md](README.fr.md).*

## What is in the ROM

- **Six kinds of rooms**, one visual identity: dig sites (one dig per row,
  column and rock, never touching), ore veins to balance, prospector ledgers
  (each symbol once per row, column and zone), continuous galleries through
  every cell with numbered exits, cavities to fill with stone blocks, and
  quick nugget-prospecting bonus rooms.
- **A branching descent** of 30 layers: at each node the map shows the next
  rooms with their type, a difficulty gauge and their reward (ore, an extra
  hint, a life, a risky room paying double, a camp to rest). Difficulty rises
  in a sawtooth and always starts gently.
- **Run resources**: lives, hint tokens, ore. Every visible mistake spends a
  room's stability; an empty budget caves the room in and costs a life.
  Hints apply one real deduction step, never the whole solution.
- **Permanent progression**, earned only by playing: stats, a logbook and
  three powers unlocked by milestones (an extra hint per run, an extra life,
  a free first cave-in).
- **Battery saves**: the run in progress (down to the board, cursor and
  counters of the current room) and the permanent profile are two
  independent, checksummed SRAM blocks. *Continue* reopens the exact room.
- **Puzzle banks** generated on the PC: every puzzle has a single solution,
  a measured 1–10 difficulty and no near-duplicate under the symmetries of the
  square. ~1760 puzzles across five families, 60 KB of ROM.

Placeholder art and PSG sound effects for now; the pipeline is built so final
art and music replace them without code changes ([docs/assets.md](docs/assets.md)).

## Build

Pure C on devkitARM + libtonc, no assembler. From an MSYS2 shell with devkitPro
installed (`DEVKITPRO=/opt/devkitpro`), Python 3 with Pillow on the PATH:

    make            # build/deeper.gba
    make test       # host unit tests (rules, solvers, generators, engine logic)
    make emutest    # scenarios played in mGBA (needs a build with --script)
    make check      # both
    make puzzles    # regenerate data/puzzles/*.bin (deterministic seeds)
    make assets     # redraw the placeholder PNGs
    make run        # launch in mGBA

## Controls

| Key | Map | Room |
|---|---|---|
| D-pad | choose the next room | move the cursor |
| A | descend | primary action (dig, ore, symbol, block, next gallery cell) |
| B | | secondary action (note, erase, take back, back up) |
| L | | use a hint token |
| R | | family action (next block in cavity rooms) |
| SELECT | | give the room up (costs a life); on the title: sound on/off |
| START | descend | confirm |

## Repository

    common/          puzzle rules, packing and solvers, shared by the ROM, the generator and the tests
    source/          GBA engine: screens, rendering, save, run map, one adapter per family
    include/         headers
    tools/puzzlegen/ PC generator (one file per family) -> data/puzzles/*.bin
    tools/           png2gba.py, bin2c.py, make_assets.py
    assets/          source PNGs (placeholders)
    data/puzzles/    committed puzzle banks (docs/puzzle_bank.md)
    tests/unit/      host tests; tests/emu/ mGBA scenarios (tests/README.md)
    docs/            design notes (French), formats, asset pipeline

Design notes: [docs/design.md](docs/design.md) (French).

## Licence

MIT.
