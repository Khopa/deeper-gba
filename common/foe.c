#include "foe.h"

uint32_t foe_rand(uint32_t *rng)
{
    uint32_t x = *rng ? *rng : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *rng = x;
}

int foe_for_biome(int biome, uint32_t *rng)
{
    switch (biome) {
    case 0:  return FOE_GOBLIN;                                    // earth
    case 1:  return (foe_rand(rng) & 1) ? FOE_ORC : FOE_GOBLIN;    // rock: both
    case 2:  return FOE_TROLL;                                     // ice
    case 3:  return FOE_DEMON;                                     // lava
    case 4:  return FOE_TROLL;                                     // crystal
    default: return FOE_DEMON;                                     // the core
    }
}

// easy, normal, hard, very hard
static const FoeStats base[FOE_COUNT] = {
    [FOE_GOBLIN] = { 3, 3, 4 * 60, 35 },
    [FOE_ORC]    = { 5, 4, 3 * 60 + 30, 50 },
    [FOE_TROLL]  = { 7, 5, 3 * 60, 65 },
    [FOE_DEMON]  = { 9, 6, 2 * 60 + 30, 80 },
};

void foe_stats(int foe, int difficulty, FoeStats *out)
{
    if (foe < 0 || foe >= FOE_COUNT) foe = FOE_GOBLIN;
    *out = base[foe];
    int d = clamp_difficulty(difficulty);
    out->hp = (uint8_t)(out->hp + d / 4);                              // +0..2
    out->seq_len = (uint8_t)(out->seq_len + d / 5);                    // +0..2
    if (out->seq_len > FOE_SEQ_MAX) out->seq_len = FOE_SEQ_MAX;
    out->hit_pct = (uint8_t)(out->hit_pct + d);                        // +1..10
    if (out->hit_pct > 95) out->hit_pct = 95;
    out->attack_frames = (uint16_t)(out->attack_frames - d * 4);       // a touch faster deep down
}

void foe_sequence(uint32_t *rng, int len, uint8_t *out)
{
    if (len > FOE_SEQ_MAX) len = FOE_SEQ_MAX;
    for (int i = 0; i < len; i++) {
        uint8_t k;
        do k = (uint8_t)(foe_rand(rng) % FK_COUNT);
        while (i >= 2 && out[i - 1] == k && out[i - 2] == k);
        out[i] = k;
    }
}

bool foe_strike_lands(uint32_t *rng, const FoeStats *st)
{
    return (int)(foe_rand(rng) % 100) < st->hit_pct;
}
