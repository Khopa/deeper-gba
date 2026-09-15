// Music: 8-bit signed PCM tracks streamed from ROM to DirectSound A by DMA1,
// paced by timer 0. No mixing, no buffer: the DMA reads the sample array
// directly; the per-frame update counts elapsed samples to loop or stop.
// Tracks come from assets/music/*.wav through tools/wav2gba.py.
#include "sound.h"
#include "music.h"
#ifndef HOST_TEST
#include "mus_title.h"
#include "mus_mine.h"
#include "mus_puzzle.h"
#include "mus_combat.h"
#include "mus_jingle_victory.h"
#include "mus_jingle_defeat.h"
#endif

#define MUSIC_TIMER_DIV 1596                 // 16777216 / 1596 = 10512 Hz, the wav2gba default
#define MUSIC_RATE (16777216 / MUSIC_TIMER_DIV)
#define FRAMES_PER_SEC_X256 15290            // 59.7275 fps in 24.8 fixed point

typedef struct {
    const signed char *data;
    u32  len;                                // samples
    bool loop;
} Track;

#ifndef HOST_TEST
static const Track tracks[MUS_COUNT] = {
    [MUS_NONE]    = { 0, 0, false },
    [MUS_MAP]      = { mus_title, mus_title_len, mus_title_loop },
    [MUS_PUZZLE_A] = { mus_mine, mus_mine_len, mus_mine_loop },
    [MUS_PUZZLE_B] = { mus_puzzle, mus_puzzle_len, mus_puzzle_loop },
    [MUS_FIGHT]    = { mus_combat, mus_combat_len, mus_combat_loop },
    [MUS_VICTORY] = { mus_jingle_victory, mus_jingle_victory_len, mus_jingle_victory_loop },
    [MUS_DEFEAT]  = { mus_jingle_defeat, mus_jingle_defeat_len, mus_jingle_defeat_loop },
};
#else
// the host has no sample data: every track is a short silent placeholder
static const signed char host_silence[4096];
static const Track tracks[MUS_COUNT] = {
    [MUS_NONE] = { 0, 0, false }, [MUS_MAP] = { host_silence, 4096, true },
    [MUS_PUZZLE_A] = { host_silence, 4096, true }, [MUS_PUZZLE_B] = { host_silence + 8, 4096, true },
    [MUS_FIGHT] = { host_silence, 4096, true },
    [MUS_VICTORY] = { host_silence, 1024, false }, [MUS_DEFEAT] = { host_silence, 1024, false },
};
#endif

static const Track *playing;                // NULL when silent
static u32 pos_x8;                          // samples played, 24.8 fixed point
static u32 step_x8;                         // samples per frame, 24.8
static bool music_on = true;

static void dma_start(const Track *t)
{
    REG_DMA1CNT = 0;
    REG_DMA1SAD = (u32)t->data;
    REG_DMA1DAD = (u32)&REG_FIFO_A;
    REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
    pos_x8 = 0;
}

static void dma_stop(void)
{
    REG_DMA1CNT = 0;
}

void music_init(void)
{
    // DirectSound A at full volume on both sides, fed by timer 0, FIFO reset
    REG_SNDDSCNT |= SDS_A100 | SDS_AR | SDS_AL | SDS_ATMR0 | SDS_ARESET;
    REG_TM0D = (u16)(65536 - MUSIC_TIMER_DIV);
    REG_TM0CNT = TM_ENABLE;
    step_x8 = (u32)(((u64)MUSIC_RATE << 8) * 256 / FRAMES_PER_SEC_X256);
    playing = 0;
    dma_stop();
}

void music_start(MusicId id)
{
    if (id <= MUS_NONE || id >= MUS_COUNT || !tracks[id].data) { music_stop(); return; }
    const Track *t = &tracks[id];
    if (playing && playing->data == t->data) { playing = t; return; }   // same samples (biomes share tracks): keep going
    playing = t;
    if (music_on) dma_start(t);
}

void music_stop(void)
{
    playing = 0;
    dma_stop();
}

void music_enable(bool on)
{
    music_on = on;
    if (!on) dma_stop();
    else if (playing) dma_start(playing);
}

bool music_playing(void) { return playing != 0 && music_on; }

void music_update(void)
{
    if (!playing || !music_on) return;
    pos_x8 += step_x8;
    if ((pos_x8 >> 8) >= playing->len) {
        if (playing->loop) dma_start(playing);   // a few samples of gap at the seam
        else music_stop();
    }
}
