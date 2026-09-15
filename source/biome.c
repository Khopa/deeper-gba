#include "biome.h"
#include "lang.h"
#include "run.h"

#define CLR(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

static const BiomeInfo biomes[BIOME_COUNT] = {
    [BIOME_EARTH]   = { CLR(4, 3, 2),  CLR(30, 25, 8),  STR_BIOME_EARTH },
    [BIOME_ROCK]    = { CLR(3, 3, 4),  CLR(24, 24, 26), STR_BIOME_ROCK },
    [BIOME_ICE]     = { CLR(2, 4, 7),  CLR(18, 26, 31), STR_BIOME_ICE },
    [BIOME_LAVA]    = { CLR(6, 1, 1),  CLR(31, 14, 4),  STR_BIOME_LAVA },
    [BIOME_CRYSTAL] = { CLR(4, 2, 6),  CLR(26, 16, 31), STR_BIOME_CRYSTAL },
    [BIOME_CORE]    = { CLR(7, 2, 0),  CLR(31, 22, 6),  STR_BIOME_CORE },
};

// The bands a descent crosses depend on its length: the short one stays in
// the earth and the rock (goblins, orcs), the middle one adds the ice and the
// crystal (trolls), the long one goes through the lava into the core itself
// (fire demons). The last layer is always the core.
int biome_for_layer(int layer, int layers)
{
    static const uint8_t short_bands[2]  = { BIOME_EARTH, BIOME_ROCK };
    static const uint8_t middle_bands[4] = { BIOME_EARTH, BIOME_ROCK, BIOME_ICE, BIOME_CRYSTAL };
    static const uint8_t long_bands[6]   = { BIOME_EARTH, BIOME_ROCK, BIOME_ICE, BIOME_LAVA, BIOME_CRYSTAL, BIOME_CORE };
    if (layer >= layers - 1) return BIOME_CORE;
    const uint8_t *bands;
    int count;
    if (layers <= 15)      { bands = short_bands;  count = 2; }
    else if (layers <= 30) { bands = middle_bands; count = 4; }
    else                   { bands = long_bands;   count = 6; }
    int band = layer * count / (layers - 1);
    return bands[clampi(band, 0, count - 1)];
}

const BiomeInfo *biome_info(int biome) { return &biomes[clampi(biome, 0, BIOME_COUNT - 1)]; }
