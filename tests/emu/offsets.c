// Prints the byte offsets the Lua scenarios need to read the run state from
// RAM (one "name=value" per line). Compiled and run by run.py with the host
// compiler, so the Lua side never hard-codes a struct layout.
#include <stddef.h>
#include <stdio.h>
#include "run.h"
#include "save.h"

#define P(prefix, type, field) printf(#prefix "." #field "=%zu\n", offsetof(type, field))

int main(void)
{
    P(run, RunState, seed);
    P(run, RunState, layer);
    P(run, RunState, slot);
    P(run, RunState, lives);
    P(run, RunState, hints);
    P(run, RunState, ore);
    P(run, RunState, path);
    P(run, RunState, node);
    P(run, RunState, room_in_progress);
    P(run, RunState, layers);
    P(run, RunState, length_index);
    P(run, RunState, frames);
    P(profile, Profile, lengths_unlocked);
    P(profile, Profile, best_frames);
    P(profile, Profile, last_frames);
    P(profile, Profile, runs_won);
    P(profile, Profile, ore_bank);
    P(profile, Profile, upgrade);
    P(profile, Profile, cosmetics);
    P(run, RunState, props);
    printf("run.node_stride=%zu\n", sizeof(RunNode));
    P(node, RunNode, family);
    P(node, RunNode, kind);
    P(node, RunNode, difficulty);
    P(node, RunNode, puzzle);
    return 0;
}
