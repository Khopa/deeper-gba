// Key handling on top of libtonc's key_poll()/key_hit().
#include "input.h"

#define REPEAT_DELAY 18     // frames before auto-repeat starts
#define REPEAT_RATE   5     // frames between repeats

static u32 held_frames;     // how long the current D-pad state has been held
static u32 prev_dpad;

void input_poll(void)
{
    key_poll();
    u32 dpad = key_curr_state() & KEY_DIR;
    if (dpad && dpad == prev_dpad)
        held_frames++;
    else
        held_frames = 0;
    prev_dpad = dpad;
}

u32 input_hit(u32 keys)  { return key_hit(keys); }
u32 input_down(u32 keys) { return key_is_down(keys); }

u32 input_nav(u32 keys)
{
    u32 hit = key_hit(keys);
    if (hit) return hit;
    if (held_frames >= REPEAT_DELAY && (held_frames - REPEAT_DELAY) % REPEAT_RATE == 0)
        return key_curr_state() & keys & KEY_DIR;
    return 0;
}
