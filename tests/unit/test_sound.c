// Placeholder PSG effects and biome bands.
#include "test.h"
#include "sound.h"
#include "music.h"
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
    CHECK(REG_SNDDSCNT & SDS_DMG100);
    CHECK(REG_SNDDSCNT & SDS_A100);            // DirectSound A for the music
    CHECK_EQ(REG_TM0CNT, TM_ENABLE);
    CHECK_EQ(REG_TM0D, 65536 - 1596);
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

TEST(music_streams_by_dma_loops_and_stops)
{
    sound_init();
    CHECK(!music_playing());
    CHECK_EQ(REG_DMA1CNT, 0);
    music_play(MUS_PUZZLE_A);
    CHECK_EQ(music_current(), MUS_PUZZLE_A);
    CHECK(music_playing());
    CHECK(REG_DMA1CNT & DMA_ENABLE);
    CHECK(REG_DMA1CNT & DMA_REPEAT);
    CHECK(REG_DMA1CNT & DMA_AT_FIFO);
    u32 src = REG_DMA1SAD;
    CHECK(src != 0);
    CHECK_EQ(REG_DMA1DAD, (u32)&REG_FIFO_A);
    // the same track again does not restart the DMA; another one does
    REG_DMA1SAD = 0;
    music_play(MUS_PUZZLE_A);
    CHECK_EQ(REG_DMA1SAD, 0);
    music_play(MUS_PUZZLE_B);
    CHECK(REG_DMA1SAD != 0 && REG_DMA1SAD != src);
    music_play(MUS_PUZZLE_A);
    CHECK_EQ(REG_DMA1SAD, src);
    // a looping track is re-armed once its samples ran out (4096 at ~176/frame)
    for (int f = 0; f < 30; f++) sound_update();
    CHECK_EQ(REG_DMA1SAD, src);
    CHECK(music_playing());
    // a jingle ends by itself
    music_play(MUS_VICTORY);
    for (int f = 0; f < 30; f++) sound_update();
    CHECK(!music_playing());
    CHECK_EQ(REG_DMA1CNT, 0);
    // the sound switch silences and resumes the music
    music_play(MUS_MAP);
    sound_set_enabled(false);
    CHECK_EQ(REG_DMA1CNT, 0);
    CHECK(!music_playing());
    sound_set_enabled(true);
    CHECK(music_playing());
    CHECK(REG_DMA1CNT & DMA_ENABLE);
    music_play((MusicId)99);
    CHECK_EQ(music_current(), MUS_NONE);
    CHECK(!music_playing());
}

TEST(biomes_follow_depth_and_end_at_the_core)
{
    for (int li = 0; li < RUN_LENGTHS; li++) {
        int layers = run_length(li);
        CHECK_EQ(biome_for_layer(0, layers), BIOME_EARTH);
        CHECK_EQ(biome_for_layer(layers - 1, layers), BIOME_CORE);
        // the bands depend on the length: earth and rock only on the short descent,
        // ice and crystal added on the middle one, lava and the core band on the long one
        CHECK_EQ(biome_for_layer(layers - 2, layers), layers <= 15 ? BIOME_ROCK : layers <= 30 ? BIOME_CRYSTAL : BIOME_CORE);
        int prev = -1;
        bool seen[BIOME_COUNT] = {0};
        for (int l = 0; l < layers; l++) {
            int b = biome_for_layer(l, layers);
            CHECK(b >= prev);                // never goes back up
            prev = b;
            seen[b] = true;
            CHECK(biome_info(b)->name_str != 0);
        }
        CHECK(seen[BIOME_EARTH] && seen[BIOME_ROCK] && seen[BIOME_CORE]);
        CHECK_EQ(seen[BIOME_ICE], layers > 15);
        CHECK_EQ(seen[BIOME_CRYSTAL], layers > 15);
        CHECK_EQ(seen[BIOME_LAVA], layers > 30);
    }
}

const TestCase sound_tests[] = {
    T(init_enables_the_psg_and_silences_channels),
    T(every_effect_makes_a_sound_and_ends),
    T(solved_uses_two_squares_and_place_uses_noise),
    T(off_switch_silences_everything),
    T(music_streams_by_dma_loops_and_stops),
    T(biomes_follow_depth_and_end_at_the_core),
};
SUITE(sound)
