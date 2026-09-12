#include "rng.h"

static u32 state = 0x2545F491u;

void rng_seed(u32 seed)
{
    state = seed ? seed : 0x2545F491u;
}

u32 rng_next(void)
{
    u32 x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return state = x;
}

u32 rng_state(void)
{
    return state;
}

u32 rng_range(u32 n)
{
    return (rng_next() >> 8) % n;
}
