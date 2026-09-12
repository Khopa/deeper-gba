// A run: the branching map from the surface to the core, the resources that
// live only for this descent (lives, hints, ore) and the choices made so
// far. Pure logic, no rendering; the whole struct is what gets saved.
#ifndef RUN_H
#define RUN_H

#include "common.h"

#define RUN_LAYERS 30           // layer 0 = surface, RUN_LAYERS-1 = the core
#define RUN_SLOTS  3            // node columns per layer
#define RUN_NO_SLOT 0xFF
#define RUN_MAX_LIVES 5

// What a node offers besides its puzzle
enum NodeKind {
    NODE_PUZZLE = 0,            // plain room: ore reward
    NODE_RISKY,                 // harder, low stability, double ore
    NODE_HINT,                  // clearing it grants a hint token
    NODE_LIFE,                  // clearing it grants a life
    NODE_CAMP,                  // no puzzle: rest, +1 life
    NODE_CORE,                  // the final room
    NODE_KIND_COUNT
};

typedef struct {
    u8  present;                // 0 when the slot is empty in this layer
    u8  family;                 // enum PuzzleFamily (NUGGET rooms have no bank puzzle)
    u8  kind;                   // enum NodeKind
    u8  difficulty;             // 1..10
    u16 puzzle;                 // index in the family's bank
} RunNode;

typedef struct {
    u32 seed;
    u8  layer, slot;            // where the dwarf stands
    u8  lives, hints;
    u16 ore;
    u8  max_lives;
    u8  path[RUN_LAYERS];       // slot visited per layer (RUN_NO_SLOT = not yet)
    u16 edges[RUN_LAYERS];      // bit (from * 3 + to): edge from slot `from` to slot `to` of the next layer
    RunNode node[RUN_LAYERS][RUN_SLOTS];
    u8  room_in_progress;       // 1 when a room state follows in the save
} RunState;

// Which families the map may use (the engine sets the ones with an adapter + bank)
void run_set_available_families(const bool available[FAM_COUNT]);

// Build a fresh run: map, resources, first node. `recent` (may be NULL) lists
// (family << 12 | index) keys of recently played puzzles to avoid.
void run_new(RunState *rs, u32 seed, const u16 *recent, int n_recent);

// Reachable slots from the current node (bitmask over RUN_SLOTS), 0 at the core
int  run_next_choices(const RunState *rs);
bool run_can_go(const RunState *rs, int slot);
void run_go(RunState *rs, int slot);                 // move to (layer+1, slot)
const RunNode *run_current(const RunState *rs);
bool run_at_core(const RunState *rs);

// Resource effects of room outcomes
void run_room_cleared(RunState *rs, int ore_gained);
void run_room_failed(RunState *rs);                  // collapse or abandon: lose a life
bool run_is_over(const RunState *rs);                // no lives left

// Difficulty window used by the map builder (exposed for tests): sawtooth
// rising with depth, very gentle for the first layers.
int  run_base_difficulty(int layer);
// Stability (mistake budget) for a node kind
int  run_stability(const RunNode *n);
// Ore a node promises (before hint/mistake penalties)
int  run_reward_ore(const RunNode *n);

#endif
