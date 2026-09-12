// Small xorshift32 generator (no libc rand on the GBA).
#ifndef RNG_H
#define RNG_H

#include "common.h"

void rng_seed(u32 seed);
u32  rng_next(void);
u32  rng_state(void);
u32  rng_range(u32 n);      // uniform-ish in [0, n)

#endif
