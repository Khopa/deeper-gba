// Family adapter interface: every puzzle family exposes the same handful of
// operations so the room screen (source/room.c) can drive any of them with
// the same controls and feedback. Adapters are pure logic (no rendering,
// no hardware) so they run in the host tests.
#ifndef PUZZLE_H
#define PUZZLE_H

#include "common.h"

// Cursor actions: A = primary (place / cycle), B = secondary (note / mark)
enum { ACT_A = 0, ACT_B = 1 };

typedef struct {
    bool changed;        // the board changed (redraw)
    bool mistake;        // the action created an error the player can see
    bool solved;         // the board is complete and correct
} ActionResult;

enum HintResult { HINT_APPLIED = 0, HINT_WRONG_PLACEMENT, HINT_NOTHING };

typedef struct {
    u8 variant;          // cell tile set: 0 rock (edges = thick borders), 1 dug gallery (edges = links)
    u8 edges;            // thick edges (1 N, 2 E, 4 S, 8 W)
    u8 pal;              // palette bank for the cell tiles
    u8 mark;             // overlay (MARK_*)
    u8 conflict;         // 1 when this cell is currently wrong
} CellView;

#define ROOM_STATE_MAX 96    // bytes a family may need to save a room in progress

typedef struct {
    int  family;
    int  name_str;       // STR_ROOM_*
    int  help_str;       // one-line rules reminder
    int  key_a_str, key_b_str;

    bool (*load)(const PuzzleHeader *h, const uint8_t *payload);
    int  (*size)(void);                                  // grid side
    void (*cell)(int r, int c, CellView *out);
    ActionResult (*action)(int r, int c, int act);
    bool (*solved)(void);
    int  (*hint)(int *r, int *c);                        // applies one step, returns HintResult
    int  (*save)(uint8_t *buf);                          // returns bytes written (<= ROOM_STATE_MAX)
    bool (*restore)(const uint8_t *buf, int len);
    // optional (may be NULL)
    void (*aux)(void);                                   // R button: family-specific secondary action
    int  (*tray)(uint8_t *rc, int max);                  // small shape to preview (row, col pairs); returns cells
    int  key_r_str;                                      // label for R when aux exists
    // Bonus rooms: misses spend the stability budget but an empty budget ends
    // the room instead of caving it in, and the reward is what was collected.
    int  (*bonus_ore)(void);
} PuzzleOps;

extern const PuzzleOps ops_dig;
extern const PuzzleOps ops_vein;
extern const PuzzleOps ops_ledger;
extern const PuzzleOps ops_tunnel;
extern const PuzzleOps ops_block;
extern const PuzzleOps ops_nugget;

const PuzzleOps *puzzle_ops(int family);   // NULL for families without an adapter yet

#endif
