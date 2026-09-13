// Battery-backed SRAM: two independent blocks, each with magic, version and
// checksum so a corrupted or blank cartridge falls back to defaults.
//   block 0 (profile)  permanent progression: stats, unlocked powers, options,
//                      recently played puzzles
//   block 1 (run)      the run in progress, plus the room in progress if any
#ifndef SAVE_H
#define SAVE_H

#include "common.h"
#include "run.h"
#include "puzzle.h"

#define RECENT_MAX 64

// Permanent powers (bitmask in Profile.powers). Aids to reasoning, never
// solvers: they hand out resources or soften a penalty.
enum Power {
    POWER_LAMP = 0,          // +1 hint token at the start of every run
    POWER_TOUGH,             // +1 life at the start of every run
    POWER_SECOND_CHANCE,     // the first cave-in of a run costs no life
    POWER_COUNT
};

typedef struct {
    u16 runs_started, runs_won;
    u16 best_depth;          // deepest layer reached + 1
    u32 total_ore;           // ore brought back over every run
    u16 ore_bank;            // spendable permanent currency (future shop)
    u8  lengths_unlocked;    // 1..RUN_LENGTHS: how many descent lengths may be chosen
    u32 best_frames[RUN_LENGTHS];   // fastest victory per length (0 = none yet)
    u32 last_frames;         // duration of the last run, won or lost
    u32 total_frames;        // time spent in runs overall
    u32 powers;              // bitmask of enum Power
    u8  lang, sound;
    u8  recent_head;
    u16 recent[RECENT_MAX];  // (family << 12 | puzzle index), ring buffer
} Profile;

typedef struct {
    u8  len;                 // 0 = no room in progress
    u8  stability, hints_left, mistakes, hints_used;
    u8  cur_r, cur_c;
    u16 elapsed;             // frames spent in the room so far
    u8  data[ROOM_STATE_MAX];
} RoomSave;

void     save_init(void);                   // read SRAM, validate, fall back to defaults
Profile *save_profile(void);
void     save_profile_commit(void);
void     save_profile_add_recent(int family, int index);

bool     save_has_run(void);
void     save_run_commit(const RunState *rs, const RoomSave *room);   // room may be NULL
bool     save_run_load(RunState *rs, RoomSave *room);
void     save_run_clear(void);

// Powers granted by milestones; returns a bitmask of newly unlocked powers
u32      save_check_unlocks(void);

// Exposed for tests: block layout
#define SAVE_PROFILE_OFF 0x0000
#define SAVE_RUN_OFF     0x0400
#define SAVE_MAGIC       0x44505A31u      // "DPZ1"
#define SAVE_VERSION     2

#endif
