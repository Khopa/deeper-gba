# Tests

Two layers, both runnable with one command and both exit non-zero on failure:

| Command | What runs | Time |
|---|---|---|
| `make test` | host unit tests (`tests/unit`) — rules, solvers, generators, committed banks, engine logic compiled on the PC | < 1 s |
| `make emutest` | scenarios played in mGBA (`tests/emu`) — the real ROM, driven by Lua | ~10 s |
| `make check` | both | |

## Unit tests (`tests/unit`)

`common/*.c`, the family adapters, `bank.c`, `run.c`, `save.c`, `lang.c`,
`sound.c`, `biome.c` and the generators are compiled with the host `gcc` and
`-DHOST_TEST`. In that mode `include/common.h` pulls in
`tests/unit/host_shim.h` instead of libtonc: I/O registers become slots of a
`host_io[]` array and cartridge SRAM becomes `host_sram[]`.

Framework: `tests/unit/test.h` — a test is a `TEST(name)` function using
`CHECK(cond)`, `CHECK_EQ(actual, expected)` or `CHECK_MEM(a, b, n)`; a suite
lists its tests in a `TestCase` array and `SUITE(name)` generates the runner;
`main.c` calls every suite. Each test starts from a clean shim.

```
build/unit_tests            # run all
build/unit_tests -v         # print every test name
build/unit_tests -f vein    # only tests whose suite or name contains "vein"
make test TESTFLAGS="-f save -v"
```

| Suite | Covers |
|---|---|
| `dig` | rules (rows/columns/regions/adjacency, crosses), packing, uniqueness counter, deduction solver and hint steps on a fixture, generator properties (valid, unique, deducible, distinct, deterministic), canonical forms, hash set |
| `vein` | triples and over-full lines, packing, solver/hints, generator, committed bank soundness, adapter (givens locked, cycling, save/restore, hints) |
| `ledger` | box geometry, duplicates per unit, packing, solver/hints, generator, committed bank, adapter |
| `tunnel` | extension rules and exit order, links, packing, pruned search uniqueness, hints, generator, committed bank, adapter (dig, back up, truncate, save) |
| `block` | shape catalogue (19 free polyominoes, 88 orientations), fits, packing, exact-cover uniqueness, hints, generator, committed bank, adapter (place, turn, take back, tray) |
| `engine` | bank parsing and difficulty ranges on the committed DIG bank, DIG adapter, NUGGET bonus room, strings fit the font in both languages |
| `run` | map shape (surface, core, connectivity, no crossings) for 15/30/60 layers, branches and camps, difficulty curve per length, time budget, puzzle picking near the wanted difficulty, recent-puzzle avoidance, traversal, resources, powers |
| `save` | defaults on a blank cartridge, profile round trip and corruption, recent ring, run block with and without a room, unlock milestones |
| `sound` | PSG init, every effect sounds and ends, channel usage, off switch, music placeholder, biome bands |

Every committed bank (`data/puzzles/*.bin`) is re-verified by its suite:
each record unpacks, has exactly one solution, is solved by deduction where
the family requires it, and carries the difficulty the solver recomputes.

## Emulator scenarios (`tests/emu`)

`run.py` runs each `scenarios/*.lua` in mGBA (`--script`), on a private copy of
the ROM (`build/emu/rom.gba` + `rom.sav`) with video/audio sync disabled.
A scenario is a Lua function driven frame by frame by `lib.lua`:

```lua
-- @fresh                       -- optional: start from a blank save file
T.run(function()
  T.boot(); T.new_run()         -- title, then the entrance room
  T.solve_dig()                 -- place the solution read from the ROM bank
  T.check_eq(T.screen(), T.SCREEN.MAP, "back on the map")
  T.shot("map")                 -- tests/out/<scenario>_map.png
end)
```

Assertions read the ROM's memory rather than pixels: symbol addresses come
from the ELF (`arm-none-eabi-nm`) and struct offsets from `offsets.c`,
compiled on the host at run time. Helpers: `T.press`, `T.wait`, `T.shot`,
`T.screen()`, `T.run_state()`, `T.node()`, `T.menu_go`, `T.new_run`,
`T.solve_room` (any family, solution read from the bank in ROM), `T.abandon`,
`T.map_go`, `T.goto_cell`. `T.boot()` passes the language screen.

| Scenario | Covers |
|---|---|
| `01_boot` | title menu on a blank save (no Continue), logbook and back |
| `02_entrance_dig` | new run (length screen, 15 layers), resources, entrance solved from the bank, ore paid with a speed bonus, run clock running, map |
| `03_map_and_abandon` | Continue reopens the map, walking one layer, abandoning costs a life |
| `04_resume_mid_room` | marks placed, reset, Continue reopens the same room and it can be finished |
| `05_collapse_and_defeat` | a conflict fixed in time is free, standing ones charge after two seconds (burst), four charges cave the room in, lives run out, defeat screen, no Continue afterwards |
| `06_monkey` | 3000 random inputs: the frame counter keeps running |
| `07_full_run` | fixed seed (`debug_seed`), every room of a 15-layer descent solved from the ROM banks (all six families), core reached without losing a life, 30 layers unlocked, best/last time recorded; `tools/make_fullrun.py` replays it into `docs/fullrun.gif` |

```
python tests/emu/run.py                 # all scenarios, stop at the first failure
python tests/emu/run.py --keep-going
python tests/emu/run.py 04 -v           # one scenario, print its log
python tests/emu/run.py --slow 02       # watch it at normal speed
make emutest SCENARIO=05
```

Requirements: an mGBA build with `--script` (0.11 development builds; `--mgba`
or `MGBA=` to point at it), the devkitARM `nm`, and a host `gcc` reachable from
the shell that runs `run.py` (from MSYS2, `make emutest` takes care of it).
Screenshots and logs land in `tests/out/` (git-ignored).
