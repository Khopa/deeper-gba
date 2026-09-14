// FIREDAMP room (family NUGGET): a real minesweeper on a 6x6 rock face.
//   A: break a cell. Bare rock shows how many pockets of firedamp touch it
//      (8 neighbours; a 0 clears its neighbours). A pocket blows up: a mistake
//      charged at once, on the room's small stability.
//   B: flag a cell as dangerous (a note).
// The pockets are laid after the first break, never on or next to it. No
// bank: the layout comes from a seed (run seed ^ depth) so a saved room
// reopens identical. Rules: common/mines.c.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "mines.h"

Mines nugget_mines;                        // not static: the emulator scenarios read the pockets

static bool nugget_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_NUGGET) return false;
    uint32_t seed = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
    mines_init(&nugget_mines, seed, mines_count_for(h->difficulty));
    return true;
}

static int nugget_size(void) { return MINES_N; }

static void nugget_cell(int r, int c, CellView *out)
{
    int i = cell_at(MINES_N, r, c);
    const Mines *m = &nugget_mines;
    out->conflict = 0;
    out->edges = 0;
    if (m->cell[i] == MINES_BARE) {
        out->variant = 1;
        if (m->pocket[i]) {                        // the one that blew up
            out->pal = PAL_CELL_CONFLICT;
            out->mark = MARK_ALERT;
            out->conflict = 1;
            return;
        }
        int k = mines_adjacent(m, i);
        out->pal = PAL_REGION0 + 0;
        out->mark = k ? (u8)(MARK_DIGIT1 + k - 1) : MARK_NONE;
    } else {
        out->variant = 0;
        out->pal = PAL_REGION0 + 5;
        out->mark = m->cell[i] == MINES_FLAG ? MARK_CROSS : MARK_NONE;
    }
}

static ActionResult nugget_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int i = cell_at(MINES_N, r, c);
    Mines *m = &nugget_mines;
    if (act == ACT_B) {
        res.changed = mines_toggle_flag(m, i);
        return res;
    }
    if (m->cell[i] != MINES_HIDDEN) return res;    // bare or flagged: nothing (unflag first)
    int n = mines_break(m, i);
    res.changed = n != 0;
    res.mistake = n < 0;
    res.solved = mines_solved(m);
    return res;
}

static bool nugget_solved(void) { return mines_solved(&nugget_mines); }

// A hint bares a safe cell next to the bare area (lays the pockets first if
// the room is untouched: then any cell is safe)
static int nugget_hint(int *r, int *c)
{
    Mines *m = &nugget_mines;
    if (!m->placed) mines_place(m, MINES_CELLS / 2 + MINES_N / 2);
    int i = mines_safe_cell(m);
    if (i < 0) return HINT_NOTHING;
    if (m->cell[i] == MINES_FLAG) m->cell[i] = MINES_HIDDEN;
    mines_break(m, i);
    *r = i / MINES_N;
    *c = i % MINES_N;
    return HINT_APPLIED;
}

static int nugget_save(uint8_t *buf) { return mines_save(&nugget_mines, buf); }
static bool nugget_restore(const uint8_t *buf, int len) { return mines_restore(&nugget_mines, buf, len); }

const PuzzleOps ops_nugget = {
    .family = FAM_NUGGET,
    .name_str = STR_ROOM_NUGGET,
    .help_str = STR_HELP_NUGGET,
    .key_a_str = STR_KEY_A_BREAK,
    .key_b_str = STR_KEY_B_MARK,
    .load = nugget_load,
    .size = nugget_size,
    .cell = nugget_cell,
    .action = nugget_action,
    .solved = nugget_solved,
    .hint = nugget_hint,
    .save = nugget_save,
    .restore = nugget_restore,
    .immediate_mistakes = true,
};
