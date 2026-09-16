# Deeper

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="MIT licence"></a>
  <img src="https://img.shields.io/badge/platform-Game%20Boy%20Advance-7b68ee?style=flat-square" alt="Platform: Game Boy Advance">
  <img src="https://img.shields.io/badge/language-C-00599c?style=flat-square" alt="Written in C">
  <img src="https://img.shields.io/badge/languages-FR%20EN-f5c400?style=flat-square" alt="Two languages">
  <img src="https://img.shields.io/badge/puzzles-2908-8b5cf6?style=flat-square" alt="2908 puzzles in the banks">
  <img src="https://img.shields.io/badge/AI-Claude%20Opus%205%20--%20High-d97757?style=flat-square" alt="AI: Claude Opus 5 - High">
  <a href="https://github.com/Khopa/deeper-gba/commits/main"><img src="https://img.shields.io/github/last-commit/Khopa/deeper-gba?style=flat-square" alt="Last commit"></a>
</p>

A puzzle roguelike for the Game Boy Advance. Dwarves dig their way down through the underground, run after
run: about thirty rooms of logic puzzles, fights, and encounters on a branching map, from the surface
to the core of the earth.

<table align="center">
  <tr>
    <td align="center"><img src="assets/marketing/Cover.png" width="300" alt="Deeper, the box art: a dwarf with his pickaxe in front of the mine"></td>
    <td align="center"><img src="docs/fullrun.gif" width="480" alt="A full descent, seed 20260913, played by the test harness"></td>
  </tr>
  <tr>
    <td align="center"><em>The box art</em></td>
    <td align="center"><em>A complete 15-layer descent, surface to core, recorded from mGBA (<code>make fullrun</code>)</em></td>
  </tr>
</table>

## What is in the ROM

## Build

Pure C on devkitARM + libtonc, no assembler. Requires MSYS2 shell with devkitPro
installed (`DEVKITPRO=/opt/devkitpro`), and Python 3 with Pillow on the PATH. mGBA is required to run tests.

    make            # build/deeper.gba
    make test       # host unit tests (rules, solvers, generators, engine logic)
    make emutest    # scenarios played in mGBA (needs a build with --script)
    make check      # both
    make puzzles    # regenerate data/puzzles/*.bin (deterministic seeds)
    make assets     # redraw the placeholder PNGs
    make run        # launch in mGBA

## Process 

**Code:** Claude Opus 5 *High* (Claude Pro Plan). Single session used. 
**Music:** generated with suno.com Pro license
**Graphics:** pixellab.ai (Pixel Apprentice Tier)

+ tuning & some code & pixel art by hand.

## Controls

| Key | Map | Room |
|---|---|---|
| D-pad | choose the next room | move the cursor |
| A | descend | primary action (dig, ore, symbol, block, next gallery cell) |
| B | | secondary action (note, erase, back up; cavity: next block) |
| L | | use a hint token |
| R | | family action (cavity: turn the block; a ghost shows where it lands) |
| SELECT | | rules of the room and its controls |
| START | descend | pause menu (resume, give up for a life, save and quit) |

The game is in French and English; the language is asked at every boot
(the last choice is preselected), then the title picture leads to the menu:
Continue, New descent, Counter, Logbook, Options (sound, language).

## Repository

    common/          puzzle rules, packing and solvers, shared by the ROM, the generator and the tests
    source/          GBA engine: screens, rendering, save, run map, one adapter per family
    include/         headers
    tools/puzzlegen/ PC generator (one file per family) -> data/puzzles/*.bin
    tools/           png2gba.py, bin2c.py, make_assets.py
    assets/          source art: drawn sheets (high-res/, nodes/, dwarf/, tiles), music, marketing/ (the cover)
    data/puzzles/    committed puzzle banks (docs/puzzle_bank.md)
    tests/unit/      host tests; tests/emu/ mGBA scenarios (tests/README.md)
    docs/            design notes (French), formats, asset pipeline

Design notes: [docs/design.md](docs/design.md) (French).

## Licence

MIT.
