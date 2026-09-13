// Placeholder PSG effects and biome bands.
#include "test.h"
#include "sound.h"
#include "biome.h"
#include "run.h"
#include "host_shim.h"

static int highest_volume(int frames)
{
    int best = 0;
    for (int f = 0; f < frames; f++) {
        sound_update();
        int v1 = (REG_SND1CNT >> 12) & 15, v2 = (REG_SND2CNT >> 12) & 15, v4 = (REG_SND4CNT >> 12) & 15;
        if (v1 > best) best = v1;
        if (v2 > best) best = v2;
        if (v4 > best) best = v4;
    }
    return best;
}

TEST(init_enables_the_psg_and_silences_channels)
{
    sound_init();
    CHECK_EQ(REG_SNDSTAT, SSTAT_ENABLE);
    CHECK_EQ(REG_SNDDSCNT, SDS_DMG100);
    CHECK_EQ(REG_SND1CNT, 0);
    CHECK_EQ(REG_SND2CNT, 0);
    CHECK_EQ(REG_SND4CNT, 0);
    CHECK(sound_enabled());
}

TEST(every_effect_makes_a_sound_and_ends)
{
    for (int id = 0; id < SFX_COUNT; id++) {
        sound_init();
        sound_set_enabled(true);
        sfx_play((SfxId)id);                 // the first step is programmed at once
        int v = ((REG_SND1CNT >> 12) & 15) | ((REG_SND2CNT >> 12) & 15) | ((REG_SND4CNT >> 12) & 15);
        CHECK(v > 0);
        highest_volume(120);                 // let it run out
        CHECK_EQ(REG_SND1CNT, 0);
        CHECK_EQ(REG_SND2CNT, 0);
        CHECK_EQ(REG_SND4CNT, 0);
    }
}

TEST(solved_uses_two_squares_and_place_uses_noise)
{
    sound_init();
    sfx_play(SFX_SOLVED);
    sound_update(); sound_update(); sound_update(); sound_update();
    CHECK(((REG_SND1CNT >> 12) & 15) > 0);
    CHECK(((REG_SND2CNT >> 12) & 15) > 0);
    sound_init();
    sfx_play(SFX_PLACE);
    CHECK(((REG_SND4CNT >> 12) & 15) > 0);
}

TEST(off_switch_silences_everything)
{
    sound_init();
    sfx_play(SFX_SOLVED);
    sound_set_enabled(false);
    CHECK(!sound_enabled());
    CHECK_EQ(REG_SND1CNT, 0);
    CHECK_EQ(highest_volume(10), 0);
    sfx_play(SFX_ERROR);
    CHECK_EQ(highest_volume(10), 0);
    sound_set_enabled(true);
    sfx_play(SFX_ERROR);
    CHECK(highest_volume(2) > 0);
}

TEST(music_placeholder_remembers_the_track)
{
    music_play(MUS_ICE);
    CHECK_EQ(music_current(), MUS_ICE);
    music_play((MusicId)99);
    CHECK_EQ(music_current(), MUS_NONE);
}

TEST(biomes_follow_depth_and_end_at_the_core)
{
    CHECK_EQ(biome_for_layer(0), BIOME_EARTH);
    CHECK_EQ(biome_for_layer(RUN_LAYERS - 1), BIOME_CORE);
    CHECK_EQ(biome_for_layer(RUN_LAYERS - 2), BIOME_CRYSTAL);
    int prev = -1;
    bool seen[BIOME_COUNT] = {0};
    for (int l = 0; l < RUN_LAYERS; l++) {
        int b = biome_for_layer(l);
        CHECK(b >= prev);                    // never goes back up
        prev = b;
        seen[b] = true;
        CHECK(biome_info(b)->music != MUS_NONE);
    }
    for (int b = 0; b < BIOME_COUNT; b++) CHECK(seen[b]);
}

const TestCase sound_tests[] = {
    T(init_enables_the_psg_and_silences_channels),
    T(every_effect_makes_a_sound_and_ends),
    T(solved_uses_two_squares_and_place_uses_noise),
    T(off_switch_silences_everything),
    T(music_placeholder_remembers_the_track),
    T(biomes_follow_depth_and_end_at_the_core),
};
SUITE(sound)
