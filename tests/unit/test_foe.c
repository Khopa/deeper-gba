// FOE: monster stats per biome and difficulty, sequences, strikes.
#include "test.h"
#include "foe.h"

TEST(biomes_bind_their_foes)
{
    uint32_t rng = 1;
    CHECK_EQ(foe_for_biome(0, &rng), FOE_GOBLIN);
    CHECK_EQ(foe_for_biome(2, &rng), FOE_TROLL);
    CHECK_EQ(foe_for_biome(3, &rng), FOE_DEMON);
    CHECK_EQ(foe_for_biome(4, &rng), FOE_TROLL);
    CHECK_EQ(foe_for_biome(5, &rng), FOE_DEMON);
    int goblins = 0, orcs = 0;
    for (int i = 0; i < 200; i++) { int f = foe_for_biome(1, &rng); if (f == FOE_GOBLIN) goblins++; else if (f == FOE_ORC) orcs++; }
    CHECK(goblins > 50 && orcs > 50);              // the rock has both
}

TEST(stats_grow_from_goblin_to_demon_and_with_depth)
{
    FoeStats g, o, t, d, g10;
    foe_stats(FOE_GOBLIN, 1, &g);
    foe_stats(FOE_ORC, 1, &o);
    foe_stats(FOE_TROLL, 1, &t);
    foe_stats(FOE_DEMON, 1, &d);
    foe_stats(FOE_GOBLIN, 10, &g10);
    CHECK(g.hp < o.hp && o.hp < t.hp && t.hp < d.hp);
    CHECK(g.seq_len <= o.seq_len && o.seq_len <= t.seq_len && t.seq_len <= d.seq_len);
    CHECK(g.hit_pct <= o.hit_pct && o.hit_pct <= t.hit_pct && t.hit_pct <= d.hit_pct);
    CHECK(g.attack_frames > o.attack_frames && o.attack_frames > t.attack_frames && t.attack_frames > d.attack_frames);
    CHECK(g10.hp > g.hp && g10.seq_len > g.seq_len && g10.hit_pct > g.hit_pct && g10.attack_frames < g.attack_frames);
    CHECK(d.seq_len <= FOE_SEQ_MAX);
    foe_stats(FOE_DEMON, 10, &d);
    CHECK(d.seq_len <= FOE_SEQ_MAX && d.hit_pct <= 95);
}

TEST(sequences_use_every_key_and_never_triple)
{
    uint32_t rng = 42;
    int seen[FK_COUNT] = {0};
    for (int n = 0; n < 200; n++) {
        uint8_t seq[FOE_SEQ_MAX];
        foe_sequence(&rng, 8, seq);
        for (int i = 0; i < 8; i++) {
            CHECK(seq[i] < FK_COUNT);
            seen[seq[i]]++;
            if (i >= 2) CHECK(!(seq[i] == seq[i - 1] && seq[i - 1] == seq[i - 2]));
        }
    }
    for (int k = 0; k < FK_COUNT; k++) CHECK(seen[k] > 50);
    // deterministic from the seed
    uint32_t a = 7, b = 7;
    uint8_t sa[8], sb[8];
    foe_sequence(&a, 6, sa);
    foe_sequence(&b, 6, sb);
    CHECK_MEM(sa, sb, 6);
}

TEST(strikes_land_about_hit_pct_of_the_time)
{
    FoeStats st;
    foe_stats(FOE_GOBLIN, 1, &st);
    uint32_t rng = 99;
    int lands = 0;
    for (int i = 0; i < 2000; i++) lands += foe_strike_lands(&rng, &st);
    CHECK(lands > 2000 * (st.hit_pct - 8) / 100 && lands < 2000 * (st.hit_pct + 8) / 100);
}

const TestCase foe_tests[] = {
    T(biomes_bind_their_foes),
    T(stats_grow_from_goblin_to_demon_and_with_depth),
    T(sequences_use_every_key_and_never_triple),
    T(strikes_land_about_hit_pct_of_the_time),
};
SUITE(foe)
