#include "biome.h"
#include "lang.h"
#include "run.h"

#define CLR(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

static const BiomeInfo biomes[BIOME_COUNT] = {
    [BIOME_EARTH]   = { CLR(4, 3, 2),  CLR(30, 25, 8),  MUS_EARTH,   STR_BIOME_EARTH },
    [BIOME_ROCK]    = { CLR(3, 3, 4),  CLR(24, 24, 26), MUS_ROCK,    STR_BIOME_ROCK },
    [BIOME_ICE]     = { CLR(2, 4, 7),  CLR(18, 26, 31), MUS_ICE,     STR_BIOME_ICE },
    [BIOME_LAVA]    = { CLR(6, 1, 1),  CLR(31, 14, 4),  MUS_LAVA,    STR_BIOME_LAVA },
    [BIOME_CRYSTAL] = { CLR(4, 2, 6),  CLR(26, 16, 31), MUS_CRYSTAL, STR_BIOME_CRYSTAL },
    [BIOME_CORE]    = { CLR(7, 2, 0),  CLR(31, 22, 6),  MUS_CORE,    STR_BIOME_CORE },
};

int biome_for_layer(int layer, int layers)
{
    if (layer >= layers - 1) return BIOME_CORE;
    int band = layer * 5 / (layers - 1);                 // five bands before the core
    return clampi(band, BIOME_EARTH, BIOME_CRYSTAL);
}

const BiomeInfo *biome_info(int biome) { return &biomes[clampi(biome, 0, BIOME_COUNT - 1)]; }
