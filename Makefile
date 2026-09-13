# Deeper — Makefile
# Build from an MSYS2 shell with devkitPro installed (DEVKITPRO=/opt/devkitpro):
#     make            -> build/deeper.gba
#     make run        -> launch in mGBA
#     make test       -> host unit tests (tests/unit)
#     make emutest    -> scenarios in mGBA (tests/emu, needs a build with --script)
#     make check      -> both
#     make fullrun    -> re-record docs/fullrun.gif
#     make puzzlegen  -> build/puzzlegen (PC generator)
#     make puzzles    -> regenerate data/puzzles/*.bin (deterministic)
#     make assets     -> redraw placeholder PNGs (tools/make_assets.py)
#     make clean

TARGET   := deeper
BUILD    := build
GEN      := $(BUILD)/gen

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
PREFIX   := $(DEVKITARM)/bin/arm-none-eabi-
CC       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
GBAFIX   := $(DEVKITPRO)/tools/bin/gbafix
MGBA     ?= /c/Program\ Files/mGBA/mGBA.exe
# python is not on MSYS2's PATH by default: fall back to the Windows install
PYTHON   ?= $(shell command -v python || command -v python3 || ls /c/Python3*/python.exe 2>/dev/null | tail -1 || echo py)
HOSTCC   ?= gcc

GAME_TITLE := DEEPER
GAME_CODE  := DEEP

ARCH     := -mthumb -mthumb-interwork
CFLAGS   := -g -Wall -Wextra -O2 -mcpu=arm7tdmi -mtune=arm7tdmi $(ARCH) \
            -fomit-frame-pointer -std=gnu11 \
            -Iinclude -Icommon -I$(GEN) -I$(DEVKITPRO)/libtonc/include
LDFLAGS  := -g $(ARCH) -specs=gba.specs -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS     := -L$(DEVKITPRO)/libtonc/lib -ltonc

# ---------------------------------------------------------------------------
# Generated sources
#   assets/<name>.png (+ optional assets/<name>.opts) -> build/gen/gfx_<name>.c/.h
#   data/puzzles/<fam>.bin                            -> build/gen/bank_<fam>.c/.h
# ---------------------------------------------------------------------------
GFX_NAMES := $(basename $(notdir $(wildcard assets/*.png)))
GFX_SRCS  := $(patsubst %,$(GEN)/gfx_%.c,$(GFX_NAMES))
GFX_HDRS  := $(patsubst %,$(GEN)/gfx_%.h,$(GFX_NAMES))
BANK_NAMES := $(basename $(notdir $(wildcard data/puzzles/*.bin)))
BANK_SRCS  := $(patsubst %,$(GEN)/bank_%.c,$(BANK_NAMES))
BANK_HDRS  := $(patsubst %,$(GEN)/bank_%.h,$(BANK_NAMES))

COMMON_SRCS := $(wildcard common/*.c)
SRCS := $(wildcard source/*.c)
OBJS := $(patsubst source/%.c,$(BUILD)/%.o,$(SRCS)) \
        $(patsubst common/%.c,$(BUILD)/common_%.o,$(COMMON_SRCS)) \
        $(patsubst $(GEN)/%.c,$(BUILD)/%.o,$(GFX_SRCS) $(BANK_SRCS))

.PHONY: all clean run gen assets test emutest check fullrun puzzlegen puzzles
all: $(BUILD)/$(TARGET).gba

gen: $(GFX_SRCS) $(BANK_SRCS)

assets:
	$(PYTHON) tools/make_assets.py

$(BUILD)/$(TARGET).gba: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@ -t$(GAME_TITLE) -c$(GAME_CODE) -m00
	@echo "built $@"

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/%.o: source/%.c $(GFX_HDRS) $(BANK_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/common_%.o: common/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: $(GEN)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN)/gfx_%.c $(GEN)/gfx_%.h: assets/%.png tools/png2gba.py | $(GEN)
	$(PYTHON) tools/png2gba.py $< -o $(GEN)/gfx_$* $(shell cat assets/$*.opts 2>/dev/null)

$(GEN)/bank_%.c $(GEN)/bank_%.h: data/puzzles/%.bin tools/bin2c.py | $(GEN)
	$(PYTHON) tools/bin2c.py $< -o $(GEN)/bank_$* --name bank_$*

$(BUILD) $(GEN):
	mkdir -p $@

run: $(BUILD)/$(TARGET).gba
	$(MGBA) $< &

# ---------------------------------------------------------------------------
# PC puzzle generator (tools/puzzlegen + common/)
# ---------------------------------------------------------------------------
PG_SRCS  := $(wildcard tools/puzzlegen/*.c) $(COMMON_SRCS)
PG_FLAGS := -std=gnu11 -Wall -Wextra -O2 -g -Icommon -Itools/puzzlegen

puzzlegen: $(BUILD)/puzzlegen
$(BUILD)/puzzlegen: $(PG_SRCS) $(wildcard common/*.h tools/puzzlegen/*.h) | $(BUILD)
	$(HOSTCC) $(PG_FLAGS) $(PG_SRCS) -o $@

# The seeds below are part of the data: the same command always yields the same bank.
puzzles: $(BUILD)/puzzlegen
	$(BUILD)/puzzlegen dig --count 480 --sizes 5,6,7,8 --per-diff 80 --seed 20260913 --out data/puzzles/dig.bin
	$(BUILD)/puzzlegen vein --count 320 --sizes 6,8 --per-diff 40 --seed 20260913 --out data/puzzles/vein.bin
	$(BUILD)/puzzlegen ledger --count 320 --sizes 4,6,8 --per-diff 50 --seed 20260913 --out data/puzzles/ledger.bin
	$(BUILD)/puzzlegen tunnel --count 320 --sizes 5,6,7 --per-diff 40 --seed 20260913 --max-attempts 40000 --out data/puzzles/tunnel.bin
	$(BUILD)/puzzlegen block --count 320 --sizes 5,6,7 --per-diff 40 --seed 20260913 --max-attempts 600000 --out data/puzzles/block.bin

# ---------------------------------------------------------------------------
# Tests (see tests/README.md)
# ---------------------------------------------------------------------------
# Host unit tests: common/ and the platform-independent engine modules below
# compile on the PC against tests/unit/host_shim.h (fake registers and SRAM).
UNIT_GAME_SRCS := $(COMMON_SRCS) \
                  source/bank.c source/puzzle.c source/fam_dig.c source/fam_vein.c source/fam_ledger.c source/fam_tunnel.c source/fam_block.c source/fam_nugget.c source/lang.c source/sound.c source/biome.c \
                  source/run.c source/rng.c source/save.c source/shop.c \
                  tools/puzzlegen/util.c tools/puzzlegen/gen_dig.c tools/puzzlegen/gen_vein.c tools/puzzlegen/gen_ledger.c tools/puzzlegen/gen_tunnel.c tools/puzzlegen/gen_block.c
UNIT_SRCS := $(wildcard tests/unit/*.c) $(UNIT_GAME_SRCS)
UNIT_FLAGS := -std=gnu11 -Wall -Wextra -O1 -g -DHOST_TEST -Iinclude -Icommon -Itools/puzzlegen -Itests/unit -I$(GEN)

$(BUILD)/unit_tests: $(UNIT_SRCS) $(wildcard include/*.h common/*.h tests/unit/*.h) | $(BUILD)
	$(HOSTCC) $(UNIT_FLAGS) $(UNIT_SRCS) -o $@

test: $(BUILD)/unit_tests
	$(BUILD)/unit_tests $(TESTFLAGS)

# Emulator tests: tests/emu/run.py drives the ROM in mGBA through Lua scenarios.
#   make emutest                    every scenario
#   make emutest SCENARIO=02
emutest: $(BUILD)/$(TARGET).gba
	$(PYTHON) tests/emu/run.py --rom $< $(SCENARIO)

check: test emutest

# Re-record docs/fullrun.gif (the full-run scenario played in mGBA, fixed seed)
fullrun: $(BUILD)/$(TARGET).gba
	$(PYTHON) tools/make_fullrun.py --rom $<

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
