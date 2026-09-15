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
| `heart` | clues, line solver (forced cells, contradictions), rules (conflicts, done lines, solved), packing and layout limits, generator on the drawings (each once, line-solvable, few givens), committed bank per size and difficulty window, adapter (mark, note, hint, save/restore), run wiring (core only, sized by length, dig fallback) |
| `foe` | foes per biome, stats growing from goblin to demon and with depth, sequences (every key, no triple, seeded), strike odds |
| `engine` | the firedamp room (safe first break, counts, flags, blast, save/restore, seeds, hints), bank parsing and difficulty ranges on the committed DIG bank, DIG adapter, NUGGET bonus room, strings fit the font in both languages |
| `run` | families and sizes gated by layer (cavities, firedamp, big ledgers, crates), one life of one to start, camps heal up to the run's maximum, map shape (surface, core, connectivity, no crossings) for 15/30/60 layers, branches and camps, difficulty curve per length, time budget, puzzle picking near the wanted difficulty, recent-puzzle avoidance, traversal, resources, powers |
| `save` | defaults on a blank cartridge, profile round trip and corruption, recent ring, run block with and without a room, unlock milestones |
| `sound` | PSG init, every effect sounds and ends, channel usage, off switch, PCM music by DMA (start, shared tracks, loop, jingle end, off switch), biome bands |
| `shop` | catalogue split (camp / counter), camp goods and caps (a camp life only up to the maximum), per-level pricing of permanent gear, the flask locked behind the bedroll, lantern percentages, cosmetics bits, gear applied to a new run |

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
| `01_boot` | language screen, title picture, menu without Continue, options (sound and language saved), logbook, back |
| `01_boot (old)` | title menu on a blank save (no Continue), logbook and back |
| `02_entrance_dig` | new run (length screen, 15 layers), resources, entrance solved from the bank, ore paid with a speed bonus, run clock running, map with its music streaming (DirectSound/DMA registers) |
| `03_map_and_abandon` | Continue reopens the map, walking one layer, help and pause modals stop the clock, save-and-quit keeps the room, abandoning costs a life |
| `04_resume_mid_room` | marks placed, reset, Continue reopens the same room and it can be finished |
| `05_collapse_and_defeat` | a conflict fixed in time is free, standing ones charge after two seconds (burst), four charges cave the room in, lives run out, defeat screen, no Continue afterwards |
| `06_monkey` | 3000 random inputs: the frame counter keeps running |
| `07_full_run` | fixed seed (`debug_seed`), every room of a 15-layer descent solved from the ROM banks (all six families, then the core's picture room: its help modal, the reveal), core reached without losing a life, 30 layers unlocked, best/last time recorded; `tools/make_fullrun.py` replays it into `docs/fullrun.gif` |
| `08_long_run` | every length unlocked in the profile, a 60-layer run on a fixed seed played to the core: full-length map window, biomes, save block and difficulty curve |
| `09_shop` | the counter refuses without ore, sells a satchel and both cosmetics from a written bank, the gear shows on the next run and survives a reset |
| `10_seeds_15` | three more 15-layer descents on fixed seeds (leftmost, rightmost, cycling choices), every room solved, two camps and one core each, victories and bank counted |
| `11_game_over` | the entrance caved in on purpose, the next rooms given up: defeat screen, ore banked, nothing unlocked, nothing to continue, a fresh run starts clean |
| `12_core_retry` | the heart failed with four wrong ore marks: a life lost and another picture offered, down to the last life; solving it then is the victory |
| `13_hints_and_timer` | the same entrance solved at once and after forty seconds (twenty of them paused): smaller speed bonus, clock counts play time only; hints spent even when the room is given up, none left = complaint |
| `14_node_kinds` | steering by kind on a chosen seed: hint rooms +1 token, life rooms and camps +1 life capped at five, risky rooms pay double, puzzle rooms base plus bonus |
| `15_resume_core` | save & quit in the heart with marks, a note and the cursor moved, reboot, continue: everything back, the picture finished from there |
| `17_fights` | fight nodes sought on a 60-layer run: monsters beaten from their sequences read in RAM, double reward, three kinds met, a fight lost on purpose costs a life |
| `16_english_and_options` | English picked at boot, sound off in the options, both surviving a reboot; English screens shot (menu, logbook, room, help, pause, map, counter) |

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
