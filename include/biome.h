// Geological biomes along the descent: a backdrop tint, an accent colour
// and a music track per band of depth, the core at the bottom.
#ifndef BIOME_H
#define BIOME_H

#include "common.h"
#include "sound.h"

enum Biome { BIOME_EARTH = 0, BIOME_ROCK, BIOME_ICE, BIOME_LAVA, BIOME_CRYSTAL, BIOME_CORE, BIOME_COUNT };

typedef struct {
    u16     backdrop;      // BGR555 colour behind everything
    u16     accent;        // used for the header title
    MusicId music;
    int     name_str;      // STR_BIOME_*
} BiomeInfo;

int              biome_for_layer(int layer, int layers);   // layer 0..layers-1
const BiomeInfo *biome_info(int biome);

#endif
