// FOE — monster encounters (the fight nodes of the map).
//
// A monster stands in the middle of the screen; the player beats it by
// entering the key sequences shown, one after the other, each completed
// sequence taking a hit point off it. Meanwhile the monster's attack bar
// drains; empty, it strikes, and the player dodges with some luck or loses
// one of the fight's three hearts. Four foes, each bound to biomes, from the
// easy goblin to the very hard fire demon: more hit points, longer sequences,
// faster and surer strikes. Pure logic: the fight screen (source/fightscreen.c)
// draws it, the tests run it on the host.
#ifndef FOE_H
#define FOE_H

#include "puzzle_common.h"

enum Foe { FOE_GOBLIN = 0, FOE_ORC, FOE_TROLL, FOE_DEMON, FOE_COUNT };

// Keys of a sequence
enum FoeKey { FK_UP = 0, FK_DOWN, FK_LEFT, FK_RIGHT, FK_A, FK_B, FK_L, FK_R, FK_COUNT };

#define FOE_SEQ_MAX     8
#define FOE_PLAYER_HP   3

typedef struct {
    uint8_t hp;                 // hits to land
    uint8_t seq_len;            // keys per sequence
    uint16_t attack_frames;     // the bar drains over this many frames
    uint8_t hit_pct;            // chance a strike lands (the rest is dodged)
} FoeStats;

// The foe of a biome (biomes 0..5: earth, rock, ice, lava, crystal, core);
// *rng advances (xorshift) when the biome offers two
int  foe_for_biome(int biome, uint32_t *rng);
// Stats for a foe at a node difficulty (1..10): harder nodes add hit points,
// a key or two, and make the strike a little surer
void foe_stats(int foe, int difficulty, FoeStats *out);
// A fresh sequence of `len` keys (no key three times in a row)
void foe_sequence(uint32_t *rng, int len, uint8_t *out);
// Does the strike land? (roll against hit_pct)
bool foe_strike_lands(uint32_t *rng, const FoeStats *st);
uint32_t foe_rand(uint32_t *rng);

#endif
