// Key handling: edge detection plus auto-repeat for D-pad navigation.
#ifndef INPUT_H
#define INPUT_H

#include "common.h"

void input_poll(void);          // call once per frame
u32  input_hit(u32 keys);       // pressed this frame
u32  input_down(u32 keys);      // currently held
u32  input_nav(u32 keys);       // pressed this frame, or held long enough to repeat

#endif
