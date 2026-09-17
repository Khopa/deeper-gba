# Deeper

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="MIT licence"></a>
  <img src="https://img.shields.io/badge/platform-Game%20Boy%20Advance-7b68ee?style=flat-square" alt="Platform: Game Boy Advance">
  <img src="https://img.shields.io/badge/language-C-00599c?style=flat-square" alt="Written in C">
  <img src="https://img.shields.io/badge/languages-FR%20EN%20ES%20DE-f5c400?style=flat-square" alt="Four languages">
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

## Screenshots

<table align="center">
  <tr>
    <td align="center"><img src="docs/screens/dig.png" width="240" alt="Dig site"></td>
    <td align="center"><img src="docs/screens/vein.png" width="240" alt="Ore vein"></td>
    <td align="center"><img src="docs/screens/ledger.png" width="240" alt="Prospector ledger"></td>
  </tr>
  <tr>
    <td align="center"><em>Dig site</em></td>
    <td align="center"><em>Ore vein</em></td>
    <td align="center"><em>Prospector ledger</em></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screens/tunnel.png" width="240" alt="Gallery"></td>
    <td align="center"><img src="docs/screens/block.png" width="240" alt="Cavity and its tray of blocks"></td>
    <td align="center"><img src="docs/screens/firedamp.png" width="240" alt="Firedamp face"></td>
  </tr>
  <tr>
    <td align="center"><em>Gallery</em></td>
    <td align="center"><em>Cavity and its tray of blocks</em></td>
    <td align="center"><em>Firedamp face</em></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screens/core.png" width="240" alt="The core: a picture in the ore"></td>
    <td align="center"><img src="docs/screens/fight.png" width="240" alt="A goblin, key sequences against its attack bar"></td>
    <td align="center"><img src="docs/screens/wall.png" width="240" alt="The wall: hammer A before the clock runs out"></td>
  </tr>
  <tr>
    <td align="center"><em>The core: a picture in the ore</em></td>
    <td align="center"><em>A goblin, key sequences against its attack bar</em></td>
    <td align="center"><em>The wall: hammer A before the clock runs out</em></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screens/map.png" width="240" alt="The map: choose the next room"></td>
    <td align="center"><img src="docs/screens/crates.png" width="240" alt="Three crates, one to open"></td>
    <td align="center"><img src="docs/screens/camp.png" width="240" alt="The merchant at a camp"></td>
  </tr>
  <tr>
    <td align="center"><em>The map: choose the next room</em></td>
    <td align="center"><em>Three crates, one to open</em></td>
    <td align="center"><em>The merchant at a camp</em></td>
  </tr>
</table>

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

## How it was made

| | |
|---|---|
| **Code** | [Claude Opus 5](https://claude.com) (*High* reasoning, Claude Pro plan), a single conversation from the first line to this README |
| **Music** | [Suno](https://suno.com) (Pro licence) |
| **Graphics** | [PixelLab](https://pixellab.ai) (Pixel Apprentice tier) |

Everything was then tuned by hand: game design, balance and pixel art.

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

The game is in French, English, Spanish and German; the language is asked at every boot
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
