// The wall: a rock face to break through by hammering A before the time runs
// out. Flashes, flying chips, cracks, then the wall shatters.
#ifndef MASHSCREEN_H
#define MASHSCREEN_H

#include "common.h"
#include "run.h"

enum { MASH_RUNNING = 0, MASH_WON, MASH_LOST };

int  mash_hits_needed(int difficulty);   // presses to break the wall
void mash_enter(const RunState *rs, const RunNode *node);
int  mash_update(void);                  // once per frame; the outcome once the player pressed A on the end message

// Read by the emulator scenarios
extern int mash_hits_left, mash_time_left;

#endif
