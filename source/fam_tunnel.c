// TUNNEL adapter: one gallery through every open cell, exits in order.
//   A on a cell next to the head: dig there (extend the gallery)
//   A on a cell already dug: go back to it (truncate)
//   B: back up one cell
// Illegal digs next to the head (rock, wrong exit order, last exit too
// early) count as mistakes; the gallery itself never shows conflicts.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "tunnel.h"

static TunnelPuzzle puzzle;
static TunnelBoard  board;

static bool tunnel_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_TUNNEL || !tunnel_unpack(payload, h->size, &puzzle)) return false;
    tunnel_board_init(&puzzle, &board);
    return true;
}

static int tunnel_size(void) { return puzzle.n; }

static int path_index(int cell)
{
    for (int i = 0; i < board.len; i++) if (board.path[i] == cell) return i;
    return -1;
}

static void tunnel_cell(int r, int c, CellView *out)
{
    int i = cell_at(puzzle.n, r, c);
    int v = puzzle.cell[i];
    out->conflict = 0;
    if (v == TUNNEL_ROCK) {
        out->variant = 0;
        out->edges = 15;
        out->pal = PAL_REGION0 + 5;
        out->mark = MARK_NONE;
        return;
    }
    int links = tunnel_links(&puzzle, &board, i);
    int idx = path_index(i);
    out->variant = idx >= 0 ? 1 : 0;
    out->edges = (u8)(idx >= 0 ? links : 0);
    out->pal = (u8)(idx >= 0 && idx == board.len - 1 ? PAL_CELL_HILITE : PAL_REGION0 + 0);
    out->mark = v ? (u8)(MARK_DIGIT1 + v - 1) : MARK_NONE;
}

static ActionResult tunnel_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int i = cell_at(puzzle.n, r, c);
    int head = board.path[board.len - 1];
    if (act == ACT_B) {
        if (board.len > 1) { tunnel_truncate(&board, board.len - 1); res.changed = true; }
        return res;
    }
    int idx = path_index(i);
    if (idx >= 0) {
        if (idx < board.len - 1) { tunnel_truncate(&board, idx + 1); res.changed = true; }
        return res;
    }
    if (tunnel_can_extend(&puzzle, &board, i)) {
        tunnel_extend(&board, i);
        res.changed = true;
        res.solved = tunnel_solved(&puzzle, &board);
    } else if (tunnel_step_dir(puzzle.n, head, i) >= 0) {
        res.mistake = true;                 // adjacent but forbidden
    }
    return res;
}

static bool tunnel_is_solved(void) { return tunnel_solved(&puzzle, &board); }

static int tunnel_hint(int *r, int *c)
{
    int cell;
    if (!tunnel_next_step(&puzzle, &board, &cell)) {
        if (cell < board.len) {
            *r = board.path[cell] / puzzle.n;
            *c = board.path[cell] % puzzle.n;
            return HINT_WRONG_PLACEMENT;
        }
        return HINT_NOTHING;
    }
    tunnel_extend(&board, cell);
    *r = cell / puzzle.n;
    *c = cell % puzzle.n;
    return HINT_APPLIED;
}

static int tunnel_save(uint8_t *buf)
{
    buf[0] = board.len;
    memcpy(buf + 1, board.path, board.len);
    return 1 + board.len;
}

static bool tunnel_restore(const uint8_t *buf, int len)
{
    if (len < 1 || buf[0] < 1 || buf[0] > puzzle.open_cells || len < 1 + buf[0]) return false;
    TunnelBoard b;
    tunnel_board_init(&puzzle, &b);
    if (buf[1] != b.path[0]) return false;
    for (int i = 1; i < buf[0]; i++) {
        if (!tunnel_can_extend(&puzzle, &b, buf[1 + i])) return false;
        tunnel_extend(&b, buf[1 + i]);
    }
    board = b;
    return true;
}

const PuzzleOps ops_tunnel = {
    .family = FAM_TUNNEL,
    .name_str = STR_ROOM_TUNNEL,
    .help_str = STR_HELP_TUNNEL,
    .key_a_str = STR_KEY_A_DIG,
    .key_b_str = STR_KEY_B_BACK,
    .load = tunnel_load,
    .size = tunnel_size,
    .cell = tunnel_cell,
    .action = tunnel_action,
    .solved = tunnel_is_solved,
    .hint = tunnel_hint,
    .save = tunnel_save,
    .restore = tunnel_restore,
};
