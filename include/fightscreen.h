// The fight screen: a monster in the middle, key sequences to enter, its
// attack bar draining. Logic in common/foe.c.
#ifndef FIGHTSCREEN_H
#define FIGHTSCREEN_H

#include "common.h"
#include "run.h"

enum { FIGHT_RUNNING = 0, FIGHT_WON, FIGHT_LOST };

void fight_enter(const RunState *rs, const RunNode *node);
int  fight_update(void);           // once per frame; the outcome once the player pressed A on the end message

// Read by the emulator scenarios
extern uint8_t fight_seq[8];
extern int fight_seq_len, fight_seq_pos, fight_foe_hp, fight_player_hp, fight_foe;

#endif
