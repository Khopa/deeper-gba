// PCM music player behind sound.h's music_play(): see source/music.c.
#ifndef MUSIC_H
#define MUSIC_H

#include "common.h"
#include "sound.h"

void music_init(void);
void music_start(MusicId id);      // switches track (no restart when the same track is asked again)
void music_stop(void);
void music_enable(bool on);
bool music_playing(void);
void music_update(void);           // once per frame: loops / ends the track

#endif
