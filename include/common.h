// Shared basic types. With HOST_TEST defined (tests/unit), the GBA headers
// are replaced by a shim so the engine modules compile and run on a PC.
#ifndef COMMON_H
#define COMMON_H

#ifdef HOST_TEST
#include "host_shim.h"      // tests/unit: fake registers and SRAM
#else
#include <tonc.h>
#endif

#include "puzzle_common.h"

#define SCREEN_W 240
#define SCREEN_H 160
#define TILES_W  30
#define TILES_H  20

#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

#endif
