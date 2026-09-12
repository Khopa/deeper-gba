// LEDGER adapter: symbols 1..n once per row, column and box.
//   A on a free cell: next symbol (empty -> 1 -> ... -> n -> empty)
//   B on a free cell: previous symbol
// Duplicates within a unit are shown in red; creating one is a mistake.
// Box borders are drawn as thick edges.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "ledger.h"

static LedgerPuzzle puzzle;
static LedgerBoard  board;
static uint8_t      conflict[PUZZLE_MAX_CELLS];

static void refresh_conflicts(void) { ledger_conflicts(&puzzle, &board, conflict); }

static bool ledger_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_LEDGER || !ledger_unpack(payload, h->size, &puzzle)) return false;
    ledger_board_init(&puzzle, &board);
    memset(conflict, 0, sizeof conflict);
    return true;
}

static int ledger_size(void) { return puzzle.n; }

static void ledger_cell(int r, int c, CellView *out)
{
    int n = puzzle.n, i = cell_at(n, r, c), bw = ledger_box_w(n);
    int edges = 0;
    if (r % LEDGER_BOX_H == 0) edges |= 1;
    if (c % bw == bw - 1) edges |= 2;
    if (r % LEDGER_BOX_H == LEDGER_BOX_H - 1) edges |= 4;
    if (c % bw == 0) edges |= 8;
    out->variant = 0;
    out->edges = (u8)edges;
    out->conflict = conflict[i];
    out->pal = (u8)(conflict[i] ? PAL_CELL_CONFLICT : puzzle.given[i] ? PAL_REGION0 + 5 : PAL_REGION0 + 0);
    out->mark = board.cell[i] ? (u8)(MARK_DIGIT1 + board.cell[i] - 1) : MARK_NONE;
}

static ActionResult ledger_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int n = puzzle.n, i = cell_at(n, r, c);
    if (puzzle.given[i]) return res;
    uint8_t before = board.cell[i];
    if (act == ACT_A) board.cell[i] = (uint8_t)((before + 1) % (n + 1));
    else              board.cell[i] = (uint8_t)((before + n) % (n + 1));
    res.changed = board.cell[i] != before;
    refresh_conflicts();
    res.mistake = board.cell[i] && conflict[i];
    res.solved = ledger_solved(&puzzle, &board);
    return res;
}

static bool ledger_is_solved(void) { return ledger_solved(&puzzle, &board); }

static int ledger_hint(int *r, int *c)
{
    int n = puzzle.n, cells = n * n;
    for (int i = 0; i < cells; i++)
        if (board.cell[i] && board.cell[i] != puzzle.solution[i]) {
            *r = i / n;
            *c = i % n;
            return HINT_WRONG_PLACEMENT;
        }
    int cell, value;
    if (!ledger_next_step(&puzzle, &board, &cell, &value)) return HINT_NOTHING;
    board.cell[cell] = (uint8_t)value;
    refresh_conflicts();
    *r = cell / n;
    *c = cell % n;
    return HINT_APPLIED;
}

static int ledger_save(uint8_t *buf)
{
    int cells = puzzle.n * puzzle.n, len = (cells + 1) / 2;
    memset(buf, 0, len);
    for (int i = 0; i < cells; i++) buf[i >> 1] |= (uint8_t)((board.cell[i] & 15) << ((i & 1) * 4));
    return len;
}

static bool ledger_restore(const uint8_t *buf, int len)
{
    int cells = puzzle.n * puzzle.n;
    if (len < (cells + 1) / 2) return false;
    ledger_board_init(&puzzle, &board);
    for (int i = 0; i < cells; i++) {
        int v = (buf[i >> 1] >> ((i & 1) * 4)) & 15;
        if (v > puzzle.n) return false;
        if (!puzzle.given[i]) board.cell[i] = (uint8_t)v;
    }
    refresh_conflicts();
    return true;
}

const PuzzleOps ops_ledger = {
    .family = FAM_LEDGER,
    .name_str = STR_ROOM_LEDGER,
    .help_str = STR_HELP_LEDGER,
    .key_a_str = STR_KEY_A_NEXT,
    .key_b_str = STR_KEY_B_PREV,
    .load = ledger_load,
    .size = ledger_size,
    .cell = ledger_cell,
    .action = ledger_action,
    .solved = ledger_is_solved,
    .hint = ledger_hint,
    .save = ledger_save,
    .restore = ledger_restore,
};
