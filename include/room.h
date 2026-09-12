// The puzzle room screen: grid on the left, the dwarf and the run status on
// the right, the same controls for every family (see puzzle.h).
#ifndef ROOM_H
#define ROOM_H

#include "common.h"
#include "bank.h"
#include "save.h"

enum { ROOM_RUNNING = 0, ROOM_DONE, ROOM_COLLAPSED, ROOM_ABANDONED };

typedef struct {
    int depth, max_depth;    // shown in the header
    int lives;
    int ore;
    int hints;               // hint tokens available
    int stability;           // mistakes allowed before the room collapses
    int reward_ore;          // promised ore (before penalties)
    int icon;                // node icon shown next to the title
} RoomContext;

typedef struct {
    bool solved;
    int  mistakes;
    int  hints_used;
    int  ore_gained;
} RoomResult;

// `resume` (may be NULL) restores a room saved mid-way
bool room_begin(const BankEntry *e, const RoomContext *ctx, const RoomSave *resume);
int  room_update(void);              // call once per frame after input_poll()
const RoomResult *room_result(void);
bool room_take_dirty(void);          // true once after every board change (time to save)
void room_snapshot(RoomSave *out);   // current board and counters

#endif
