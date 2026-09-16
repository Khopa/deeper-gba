// A run: the branching map from the surface to the core, the resources that
// live only for this descent (lives, hints, ore) and the choices made so
// far. Pure logic, no rendering; the whole struct is what gets saved.
#ifndef RUN_H
#define RUN_H

#include "common.h"

#define RUN_MAX_LAYERS 60       // longest descent; layer 0 = surface, layers-1 = the core
#define RUN_LENGTHS 3           // unlockable lengths, see run_length()
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
    NODE_CRATES,                // no puzzle: three crates, one empty, one small, one big
    NODE_FIGHT,                 // a monster of the biome: key sequences against its attack bar
    NODE_WALL,                  // a rock wall to dig through by mashing A before the time runs out
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
    u8  layers;                 // length of this descent (15, 30 or 60)
    u8  length_index;           // 0..RUN_LENGTHS-1
    u32 frames;                 // time spent in the run (map and rooms), in frames
    u8  path[RUN_MAX_LAYERS];   // slot visited per layer (RUN_NO_SLOT = not yet)
    u16 edges[RUN_MAX_LAYERS];  // bit (from * 3 + to): edge from slot `from` to slot `to` of the next layer
    RunNode node[RUN_MAX_LAYERS][RUN_SLOTS];
    u8  room_in_progress;       // 1 when a room state follows in the save
    u8  rope_rooms;             // rope bought at a camp: +2 stability in this many rooms to come
    u8  helmet_pct;             // helmet: chance (%) that a blow costs no life
    u8  time_bonus_pct;         // boots: extra time budget in percent
} RunState;

// Which families the map may use (the engine sets the ones with an adapter + bank)
void run_set_available_families(const bool available[FAM_COUNT]);

// Descent lengths by index: 15, 30, 60 layers
int  run_length(int index);
// Build a fresh run of run_length(length_index) layers: map, resources, first
// node. `recent` (may be NULL) lists (family << 12 | index) keys of recently
// played puzzles to avoid.
void run_new(RunState *rs, u32 seed, int length_index, const u16 *recent, int n_recent);

// Reachable slots from the current node (bitmask over RUN_SLOTS), 0 at the core
int  run_next_choices(const RunState *rs);
bool run_can_go(const RunState *rs, int slot);
void run_go(RunState *rs, int slot);                 // move to (layer+1, slot)
const RunNode *run_current(const RunState *rs);
bool run_at_core(const RunState *rs);
// Which families and sizes a layer may serve: cavities from layer 25 (and
// small ones before 40), the firedamp from layer 10, the big ledgers only
// past layer 50. Bank sizes above the cap are skipped when picking.
#define BLOCK_FIRST_LAYER   25
#define BLOCK_BIG_LAYER     40
#define NUGGET_FIRST_LAYER  10
#define LEDGER_BIG_LAYER    50
#define CRATES_FIRST_LAYER  2
#define FIGHT_FIRST_LAYER   3
#define WALL_FIRST_LAYER    1
int  run_size_cap(int family, int layer);            // 0 = no cap
// The crates' ore: the small one, the big one is three times that
int  run_crate_ore(const RunNode *n);
// After a failure at the core: another picture of the same size when there is one
void run_reroll_core(RunState *rs);

// Resource effects of room outcomes
void run_room_cleared(RunState *rs, int ore_gained);
void run_room_failed(RunState *rs, bool helmet_held);   // a cave-in (the helmet may have taken it)
bool run_is_over(const RunState *rs);                // no lives left

// Difficulty window used by the map builder (exposed for tests): sawtooth
// rising with depth over `layers`, very gentle for the first layers.
int  run_base_difficulty(int layer, int layers);
// Time budget of a room in frames; the speed bonus scales with what is left
int  run_time_budget(int difficulty);
// Stability (mistake budget) for a node kind
int  run_stability(const RunNode *n);
// Ore a node promises (before hint/mistake penalties)
int  run_reward_ore(const RunNode *n);

#endif
