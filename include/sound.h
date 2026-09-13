// Sound: placeholder PSG effects on the Game Boy tone generators (square 1,
// square 2, noise) driven by a tiny frame sequencer, and a music API that
// is a no-op for now. Final audio (sampled music/SFX) plugs in behind these
// two calls without touching the screens (see docs/assets.md).
#ifndef SOUND_H
#define SOUND_H

#include "common.h"

typedef enum {
    SFX_MOVE = 0,   // cursor moved
    SFX_PLACE,      // primary action: dig / ore / symbol / block placed
    SFX_MARK,       // secondary action: note, mark, erase
    SFX_ERROR,      // a visible mistake
    SFX_HINT,       // hint applied
    SFX_SOLVED,     // room cleared fanfare
    SFX_COLLAPSE,   // cave-in / life lost
    SFX_COLLECT,    // nugget or ore collected
    SFX_STEP,       // map: choosing / walking to a node
    SFX_POWER,      // new power unlocked
    SFX_COUNT
} SfxId;

// Music tracks: one per biome plus the map and the run endings. Placeholder:
// tracks are silent until real music exists.
typedef enum {
    MUS_NONE = 0, MUS_MAP, MUS_EARTH, MUS_ROCK, MUS_ICE, MUS_LAVA, MUS_CRYSTAL, MUS_CORE,
    MUS_VICTORY, MUS_DEFEAT, MUS_COUNT
} MusicId;

void sound_init(void);
void sound_set_enabled(bool on);
bool sound_enabled(void);
void sfx_play(SfxId id);
void music_play(MusicId id);
MusicId music_current(void);
void sound_update(void);        // once per frame

#endif
