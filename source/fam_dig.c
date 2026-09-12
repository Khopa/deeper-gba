// DIG adapter: the placement puzzle behind the room screen.
//   A on a cell: empty -> dig -> empty (a dig over a cross replaces it)
//   B on a cell: empty -> cross -> empty
// Conflicts are shown immediately (red cells); a dig that conflicts when
// placed counts as a mistake.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "dig.h"

static DigPuzzle puzzle;
static DigBoard  board;
static uint8_t   conflict[PUZZLE_MAX_CELLS];

static void refresh_conflicts(void) { dig_conflicts(&puzzle, &board, conflict); }

static bool dig_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_DIG || !dig_unpack(payload, h->size, &puzzle)) return false;
    memset(&board, 0, sizeof board);
    memset(conflict, 0, sizeof conflict);
    return true;
}

static int dig_size(void) { return puzzle.n; }

static void dig_cell(int r, int c, CellView *out)
{
    int n = puzzle.n, i = cell_at(n, r, c);
    int reg = puzzle.region[i];
    int edges = 0;
    if (r == 0 || puzzle.region[cell_at(n, r - 1, c)] != reg) edges |= 1;
    if (c == n - 1 || puzzle.region[cell_at(n, r, c + 1)] != reg) edges |= 2;
    if (r == n - 1 || puzzle.region[cell_at(n, r + 1, c)] != reg) edges |= 4;
    if (c == 0 || puzzle.region[cell_at(n, r, c - 1)] != reg) edges |= 8;
    out->edges = (u8)edges;
    out->conflict = conflict[i];
    out->pal = (u8)(conflict[i] ? PAL_CELL_CONFLICT : PAL_REGION0 + reg);
    out->mark = board.cell[i] == DIG_DIG ? MARK_DIG : board.cell[i] == DIG_CROSS ? MARK_CROSS : MARK_NONE;
}

static ActionResult dig_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int i = cell_at(puzzle.n, r, c);
    uint8_t before = board.cell[i];
    if (act == ACT_A) board.cell[i] = before == DIG_DIG ? DIG_EMPTY : DIG_DIG;
    else              board.cell[i] = before == DIG_CROSS ? DIG_EMPTY : DIG_CROSS;
    res.changed = board.cell[i] != before;
    refresh_conflicts();
    res.mistake = board.cell[i] == DIG_DIG && conflict[i];
    res.solved = dig_solved(&puzzle, &board);
    return res;
}

static bool dig_is_solved(void) { return dig_solved(&puzzle, &board); }

static int dig_hint(int *r, int *c)
{
    int cell, kind;
    int n = puzzle.n;
    // a misplaced dig blocks hints: point at it instead
    for (int i = 0; i < n * n; i++)
        if (board.cell[i] == DIG_DIG && puzzle.solution[i / n] != i % n) {
            *r = i / n;
            *c = i % n;
            return HINT_WRONG_PLACEMENT;
        }
    if (!dig_next_step(&puzzle, &board, &cell, &kind)) return HINT_NOTHING;
    board.cell[cell] = (uint8_t)kind;
    refresh_conflicts();
    *r = cell / n;
    *c = cell % n;
    return HINT_APPLIED;
}

static int dig_save(uint8_t *buf)
{
    int cells = puzzle.n * puzzle.n;
    // 2 bits per cell
    int len = (cells + 3) / 4;
    memset(buf, 0, len);
    for (int i = 0; i < cells; i++) buf[i >> 2] |= (uint8_t)((board.cell[i] & 3) << ((i & 3) * 2));
    return len;
}

static bool dig_restore(const uint8_t *buf, int len)
{
    int cells = puzzle.n * puzzle.n;
    if (len < (cells + 3) / 4) return false;
    memset(&board, 0, sizeof board);
    for (int i = 0; i < cells; i++) {
        int v = (buf[i >> 2] >> ((i & 3) * 2)) & 3;
        if (v > DIG_DIG) return false;
        board.cell[i] = (uint8_t)v;
    }
    refresh_conflicts();
    return true;
}

const PuzzleOps ops_dig = {
    .family = FAM_DIG,
    .name_str = STR_ROOM_DIG,
    .help_str = STR_HELP_DIG,
    .key_a_str = STR_KEY_A_DIG,
    .key_b_str = STR_KEY_B_MARK,
    .load = dig_load,
    .size = dig_size,
    .cell = dig_cell,
    .action = dig_action,
    .solved = dig_is_solved,
    .hint = dig_hint,
    .save = dig_save,
    .restore = dig_restore,
};

const PuzzleOps *puzzle_ops(int family)
{
    switch (family) {
    case FAM_DIG: return &ops_dig;
    default:      return NULL;
    }
}
