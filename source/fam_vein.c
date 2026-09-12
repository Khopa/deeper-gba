// VEIN adapter: two ores to balance.
//   A on a free cell: empty -> light -> dark -> empty
//   B on a free cell: erase
// Given cells are drawn in darker rock and cannot be changed. Cells in a
// triple or in an over-full line are shown in red; creating one is a mistake.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "vein.h"

static VeinPuzzle puzzle;
static VeinBoard  board;
static uint8_t    conflict[PUZZLE_MAX_CELLS];

static void refresh_conflicts(void) { vein_conflicts(&puzzle, &board, conflict); }

static bool vein_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_VEIN || !vein_unpack(payload, h->size, &puzzle)) return false;
    vein_board_init(&puzzle, &board);
    memset(conflict, 0, sizeof conflict);
    return true;
}

static int vein_size(void) { return puzzle.n; }

static void vein_cell(int r, int c, CellView *out)
{
    int i = cell_at(puzzle.n, r, c);
    out->edges = 0;
    out->conflict = conflict[i];
    out->pal = (u8)(conflict[i] ? PAL_CELL_CONFLICT : puzzle.given[i] ? PAL_REGION0 + 5 : PAL_REGION0 + 0);
    out->mark = board.cell[i] == VEIN_LIGHT ? MARK_ORE_LIGHT : board.cell[i] == VEIN_DARK ? MARK_ORE_DARK : MARK_NONE;
}

static ActionResult vein_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int i = cell_at(puzzle.n, r, c);
    if (puzzle.given[i]) return res;
    uint8_t before = board.cell[i];
    if (act == ACT_A) board.cell[i] = (uint8_t)((before + 1) % 3);
    else              board.cell[i] = VEIN_EMPTY;
    res.changed = board.cell[i] != before;
    refresh_conflicts();
    res.mistake = board.cell[i] && conflict[i];
    res.solved = vein_solved(&puzzle, &board);
    return res;
}

static bool vein_is_solved(void) { return vein_solved(&puzzle, &board); }

static int vein_hint(int *r, int *c)
{
    int n = puzzle.n, cells = n * n;
    for (int i = 0; i < cells; i++)
        if (board.cell[i] && board.cell[i] != puzzle.solution[i]) {
            *r = i / n;
            *c = i % n;
            return HINT_WRONG_PLACEMENT;
        }
    int cell, value;
    if (!vein_next_step(&puzzle, &board, &cell, &value)) return HINT_NOTHING;
    board.cell[cell] = (uint8_t)value;
    refresh_conflicts();
    *r = cell / n;
    *c = cell % n;
    return HINT_APPLIED;
}

static int vein_save(uint8_t *buf)
{
    int cells = puzzle.n * puzzle.n, len = (cells + 3) / 4;
    memset(buf, 0, len);
    for (int i = 0; i < cells; i++) buf[i >> 2] |= (uint8_t)((board.cell[i] & 3) << ((i & 3) * 2));
    return len;
}

static bool vein_restore(const uint8_t *buf, int len)
{
    int cells = puzzle.n * puzzle.n;
    if (len < (cells + 3) / 4) return false;
    vein_board_init(&puzzle, &board);
    for (int i = 0; i < cells; i++) {
        int v = (buf[i >> 2] >> ((i & 3) * 2)) & 3;
        if (v > VEIN_DARK) return false;
        if (!puzzle.given[i]) board.cell[i] = (uint8_t)v;
    }
    refresh_conflicts();
    return true;
}

const PuzzleOps ops_vein = {
    .family = FAM_VEIN,
    .name_str = STR_ROOM_VEIN,
    .help_str = STR_HELP_VEIN,
    .key_a_str = STR_KEY_A_ORE,
    .key_b_str = STR_KEY_B_ERASE,
    .load = vein_load,
    .size = vein_size,
    .cell = vein_cell,
    .action = vein_action,
    .solved = vein_is_solved,
    .hint = vein_hint,
    .save = vein_save,
    .restore = vein_restore,
};
