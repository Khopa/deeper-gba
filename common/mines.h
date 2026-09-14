// MINES — the firedamp room (family NUGGET, no bank: laid out in play).
//
// A small rock face hides pockets of firedamp. Breaking a cell reveals it: a
// pocket blows up (a mistake charged at once), bare rock shows how many
// pockets touch it (8 neighbours) and a 0 clears its neighbours too. The
// pockets are laid out after the first break, never on it or next to it, so
// the room always opens on a safe start. The room is done when every safe
// cell is bare; flags are notes only.
#ifndef MINES_H
#define MINES_H

#include "puzzle_common.h"

#define MINES_N        6
#define MINES_CELLS    (MINES_N * MINES_N)
#define MINES_MIN      4
#define MINES_MAX      8

enum MinesCell { MINES_HIDDEN = 0, MINES_BARE = 1, MINES_FLAG = 2 };

typedef struct {
    uint32_t seed;
    uint8_t  count;                        // pockets to lay
    uint8_t  placed;                       // 1 once the first break laid them
    uint8_t  pocket[MINES_CELLS];          // 1 = firedamp
    uint8_t  cell[MINES_CELLS];            // enum MinesCell
} Mines;

void mines_init(Mines *m, uint32_t seed, int count);
// Lay the pockets away from `safe` (the cell and its 8 neighbours)
void mines_place(Mines *m, int safe);
int  mines_adjacent(const Mines *m, int i);
// Break cell i: lays the pockets first if needed; returns -1 on a pocket
// (the cell is left bare so it shows), else the number of cells bared (0 if
// it was bare already; a 0-count cell bares its neighbours recursively)
int  mines_break(Mines *m, int i);
bool mines_toggle_flag(Mines *m, int i);   // false when the cell is bare
bool mines_solved(const Mines *m);
// A safe hidden cell next to a bare one (for hints), or any safe hidden cell; -1 when none
int  mines_safe_cell(const Mines *m);
// Pockets for a room's difficulty (1..10)
int  mines_count_for(int difficulty);

#define MINES_SAVE_LEN (1 + (MINES_CELLS + 7) / 8 + (MINES_CELLS + 3) / 4)
int  mines_save(const Mines *m, uint8_t *buf);
bool mines_restore(Mines *m, const uint8_t *buf, int len);

#endif
