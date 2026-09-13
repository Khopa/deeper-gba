// PSG sound effects. Each effect is up to three tracks (square 1, square 2,
// noise) of steps; every step programs the channel once and lasts a number
// of frames. Tracks end with a step of 0 frames. Same sequencer design as
// wordle-gba's sound.c.
#include "sound.h"

// --- notes -----------------------------------------------------------------
// Period values for octave 4 (C4 = 262 Hz); the tone generator plays
// 131072 / (2048 - rate) Hz, so a higher octave halves the divider.
static const u16 base_rates[12] = {
    8013, 7566, 7144, 6742, 6362, 6005, 5666, 5346, 5048, 4766, 4499, 4246,
};
enum { C = 0, CS, D, DS, E, F, FS, G, GS, A, AS, B, REST = 0xFF };

static u16 note_rate(u8 note, s8 oct)
{
    return (u16)(2048 - (base_rates[note] >> (4 + oct)));
}

// --- step tables -----------------------------------------------------------

typedef struct {
    u8 note;        // C..B or REST
    s8 oct;         // 0 = 4th octave (C4), 1 = C5, 2 = C6, -1 = C3
    u8 frames;      // 0 terminates the track
    u8 vol;         // 0-15 initial volume
    u8 decay;       // envelope step (0 = hold, 1 = fast fade ... 7 = slow)
    u8 duty;        // 0 = 12.5%, 1 = 25%, 2 = 50%, 3 = 75%
} Step;

typedef struct {
    u8 shift, ratio;   // noise clock: 524288 / ratio / 2^(shift+1)
    u8 frames;
    u8 vol, decay;
} NoiseStep;

typedef struct {
    const Step *sq1;
    const Step *sq2;
    const NoiseStep *noise;
} Sfx;

#define END_STEP  { REST, 0, 0, 0, 0, 0 }
#define END_NOISE { 0, 0, 0, 0, 0 }

static const Step move_sq1[]    = { { A, 1, 1, 4, 1, 1 }, END_STEP };
// pick strike: short click plus a burst of rock noise
static const Step place_sq1[]   = { { E, 1, 2, 9, 1, 1 }, END_STEP };
static const NoiseStep place_noise[] = { { 3, 2, 3, 8, 1 }, END_NOISE };
static const Step mark_sq1[]    = { { C, 1, 2, 6, 1, 2 }, END_STEP };
// mistake: harsh low double buzz
static const Step error_sq1[]   = { { B, -1, 6, 12, 0, 0 }, { REST, 0, 3, 0, 0, 0 }, { B, -1, 6, 12, 2, 0 }, END_STEP };
static const NoiseStep error_noise[] = { { 5, 3, 6, 6, 1 }, { 0, 0, 3, 0, 0 }, { 5, 3, 6, 6, 2 }, END_NOISE };
static const Step hint_sq1[]    = { { G, 1, 3, 8, 1, 2 }, { C, 2, 5, 8, 2, 2 }, END_STEP };
// cleared: rising fanfare with a harmony
static const Step solved_sq1[]  = { { C, 1, 6, 12, 0, 2 }, { E, 1, 6, 12, 0, 2 }, { G, 1, 6, 12, 0, 2 }, { C, 2, 24, 12, 5, 2 }, END_STEP };
static const Step solved_sq2[]  = { { REST, 0, 3, 0, 0, 0 }, { E, 1, 6, 8, 0, 2 }, { G, 1, 6, 8, 0, 2 }, { C, 2, 6, 8, 0, 2 }, { E, 2, 24, 8, 5, 2 }, END_STEP };
// cave-in: falling notes over rumbling noise
static const Step collapse_sq1[] = { { E, 0, 8, 11, 0, 2 }, { C, 0, 8, 11, 0, 2 }, { A, -1, 24, 11, 6, 2 }, END_STEP };
static const NoiseStep collapse_noise[] = { { 6, 4, 30, 10, 6 }, END_NOISE };
// collect: two bright notes
static const Step collect_sq1[] = { { E, 2, 3, 10, 1, 2 }, { A, 2, 6, 10, 3, 2 }, END_STEP };
static const Step step_sq1[]    = { { D, 0, 2, 6, 1, 1 }, END_STEP };
static const NoiseStep step_noise[] = { { 4, 3, 2, 4, 1 }, END_NOISE };
static const Step power_sq1[]   = { { G, 1, 5, 12, 0, 2 }, { B, 1, 5, 12, 0, 2 }, { D, 2, 5, 12, 0, 2 }, { G, 2, 30, 12, 6, 2 }, END_STEP };
static const Step power_sq2[]   = { { REST, 0, 5, 0, 0, 0 }, { D, 1, 5, 7, 0, 2 }, { G, 1, 5, 7, 0, 2 }, { B, 1, 30, 7, 6, 2 }, END_STEP };

static const Sfx sfx_table[SFX_COUNT] = {
    [SFX_MOVE]     = { move_sq1, 0, 0 },
    [SFX_PLACE]    = { place_sq1, 0, place_noise },
    [SFX_MARK]     = { mark_sq1, 0, 0 },
    [SFX_ERROR]    = { error_sq1, 0, error_noise },
    [SFX_HINT]     = { hint_sq1, 0, 0 },
    [SFX_SOLVED]   = { solved_sq1, solved_sq2, 0 },
    [SFX_COLLAPSE] = { collapse_sq1, 0, collapse_noise },
    [SFX_COLLECT]  = { collect_sq1, 0, 0 },
    [SFX_STEP]     = { step_sq1, 0, step_noise },
    [SFX_POWER]    = { power_sq1, power_sq2, 0 },
};

// --- sequencer -------------------------------------------------------------

typedef struct {
    const Step *steps;          // NULL when idle
    const NoiseStep *nsteps;
    u8 index;
    u8 remaining;
} Track;

static Track tracks[3];         // 0 = square 1, 1 = square 2, 2 = noise
static bool enabled = true;
static MusicId music = MUS_NONE;

static void square_set(int ch, const Step *s)
{
    u16 cnt = (u16)(SSQR_ENV_BUILD(s->vol, 0, s->decay) | ((s->duty & 3) << 6));
    u16 freq = (u16)(SFREQ_RESET | note_rate(s->note, s->oct));
    if (s->note == REST) {
        cnt = 0;
        freq = SFREQ_RESET;
    }
    if (ch == 0) {
        REG_SND1SWEEP = SSW_OFF;
        REG_SND1CNT = cnt;
        REG_SND1FREQ = freq;
    } else {
        REG_SND2CNT = cnt;
        REG_SND2FREQ = freq;
    }
}

static void noise_set(const NoiseStep *s)
{
    REG_SND4CNT = (u16)SSQR_ENV_BUILD(s->vol, 0, s->decay);
    REG_SND4FREQ = (u16)(SFREQ_RESET | (s->shift << 4) | (s->ratio & 7));
}

static void square_silence(int ch)
{
    if (ch == 0) { REG_SND1CNT = 0; REG_SND1FREQ = SFREQ_RESET; }
    else         { REG_SND2CNT = 0; REG_SND2FREQ = SFREQ_RESET; }
}

static void all_silent(void)
{
    for (int i = 0; i < 3; i++) tracks[i].steps = 0, tracks[i].nsteps = 0;
    square_silence(0);
    square_silence(1);
    REG_SND4CNT = 0;
    REG_SND4FREQ = SFREQ_RESET;
}

void sound_init(void)
{
    REG_SNDSTAT = SSTAT_ENABLE;
    REG_SNDDMGCNT = SDMG_BUILD_LR(SDMG_SQR1 | SDMG_SQR2 | SDMG_NOISE, 7);
    REG_SNDDSCNT = SDS_DMG100;
    all_silent();
}

void sound_set_enabled(bool on)
{
    enabled = on;
    if (!on) all_silent();
}

bool sound_enabled(void) { return enabled; }

static void track_start(Track *t, const Step *steps, const NoiseStep *nsteps)
{
    t->steps = steps;
    t->nsteps = nsteps;
    t->index = 0;
    t->remaining = 0;       // first step is programmed by the next update
}

void sfx_play(SfxId id)
{
    if (!enabled || id >= SFX_COUNT) return;
    const Sfx *s = &sfx_table[id];
    // a new effect replaces whatever was playing on the tracks it uses
    if (s->sq1)   track_start(&tracks[0], s->sq1, 0);
    if (s->sq2)   track_start(&tracks[1], s->sq2, 0);
    if (s->noise) track_start(&tracks[2], 0, s->noise);
    sound_update();
}

// Placeholder: remembers the requested track so screens already ask for the
// right music; nothing is played until real tracks exist.
void music_play(MusicId id) { music = id < MUS_COUNT ? id : MUS_NONE; }
MusicId music_current(void) { return music; }

void sound_update(void)
{
    for (int ch = 0; ch < 2; ch++) {
        Track *t = &tracks[ch];
        if (!t->steps) continue;
        if (t->remaining > 0) { t->remaining--; continue; }
        const Step *s = &t->steps[t->index++];
        if (s->frames == 0) {
            t->steps = 0;
            square_silence(ch);
            continue;
        }
        square_set(ch, s);
        t->remaining = (u8)(s->frames - 1);
    }

    Track *t = &tracks[2];
    if (t->nsteps) {
        if (t->remaining > 0) {
            t->remaining--;
        } else {
            const NoiseStep *s = &t->nsteps[t->index++];
            if (s->frames == 0) {
                t->nsteps = 0;
                REG_SND4CNT = 0;
                REG_SND4FREQ = SFREQ_RESET;
            } else {
                noise_set(s);
                t->remaining = (u8)(s->frames - 1);
            }
        }
    }
}
