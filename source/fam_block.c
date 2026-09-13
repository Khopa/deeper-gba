// BLOCK adapter: fit the stone blocks into the cavity.
//   A on an empty cavity cell: place the current block there (the cursor is
//      the block's top-left cell), trying its orientations until one fits
//   A on a placed block: turn it in place (next orientation that fits)
//   B on a placed block: take it back (it becomes the current block)
//   R: choose the next block to place (shown in the tray)
// Blocks never conflict: a placement that does not fit is simply refused.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "block.h"

static BlockPuzzle puzzle;
static BlockBoard  board;
static int cur;                              // piece selected for placement, -1 when all placed
static uint8_t pref[BLOCK_MAX_PIECES];        // last orientation used per piece

static int next_unplaced(int from)
{
    for (int k = 1; k <= puzzle.count; k++) {
        int j = (from + k) % puzzle.count;
        if (!board.placed[j]) return j;
    }
    return -1;
}

static bool block_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_BLOCK) return false;
    int n = h->size;
    if (!block_unpack(payload, n, (n * n + 7) / 8 + 1 + 2 * BLOCK_MAX_PIECES, &puzzle)) return false;
    block_board_init(&board);
    memset(pref, 0, sizeof pref);
    cur = 0;
    return true;
}

static int block_size(void) { return puzzle.n; }

static void block_cell(int r, int c, CellView *out)
{
    int n = puzzle.n, i = cell_at(n, r, c);
    out->conflict = 0;
    out->mark = MARK_NONE;
    if (!puzzle.cavity[i]) {
        out->variant = 0;
        out->edges = 15;
        out->pal = PAL_REGION0 + 5;
        return;
    }
    int k = block_piece_at(&puzzle, &board, i);
    if (k < 0) {
        out->variant = 1;                        // empty cavity: a dark hole
        out->edges = 0;
        out->pal = PAL_REGION0 + 0;
        return;
    }
    int edges = 0;
    if (r == 0 || block_piece_at(&puzzle, &board, cell_at(n, r - 1, c)) != k) edges |= 1;
    if (c == n - 1 || block_piece_at(&puzzle, &board, cell_at(n, r, c + 1)) != k) edges |= 2;
    if (r == n - 1 || block_piece_at(&puzzle, &board, cell_at(n, r + 1, c)) != k) edges |= 4;
    if (c == 0 || block_piece_at(&puzzle, &board, cell_at(n, r, c - 1)) != k) edges |= 8;
    out->variant = 0;
    out->edges = (u8)edges;
    out->pal = (u8)(PAL_REGION0 + 1 + (k % 7));
}

// Anchor so that the oriented shape's first cell lands on (r, c)
static int anchor_for(int piece, int orient, int r, int c)
{
    BlockShape s;
    block_shape_get(puzzle.shape[piece], orient, &s);
    int ar = r - s.r[0], ac = c - s.c[0];
    if (ar < 0 || ac < 0) return -1;
    return cell_at(puzzle.n, ar, ac);
}

static bool try_place(int piece, int r, int c, int first_orient)
{
    int no = block_shape_orients(puzzle.shape[piece]);
    for (int k = 0; k < no; k++) {
        int o = (first_orient + k) % no;
        int a = anchor_for(piece, o, r, c);
        if (a < 0 || !block_fits(&puzzle, &board, piece, o, a)) continue;
        board.placed[piece] = 1;
        board.orient[piece] = (uint8_t)o;
        board.anchor[piece] = (uint8_t)a;
        pref[piece] = (uint8_t)o;
        return true;
    }
    return false;
}

static ActionResult block_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int n = puzzle.n, i = cell_at(n, r, c);
    if (!puzzle.cavity[i]) return res;
    int k = block_piece_at(&puzzle, &board, i);
    if (act == ACT_B) {
        if (k >= 0) { board.placed[k] = 0; cur = k; res.changed = true; }
        return res;
    }
    if (k >= 0) {
        // turn in place: keep the block's first cell where it is
        int cells[BLOCK_MAX_SIZE];
        block_piece_cells(&puzzle, k, board.orient[k], board.anchor[k], cells);
        int fr = cells[0] / n, fc = cells[0] % n;
        int no = block_shape_orients(puzzle.shape[k]);
        board.placed[k] = 0;
        bool ok = false;
        for (int step = 1; step < no && !ok; step++) {
            int o = (board.orient[k] + step) % no;
            int a = anchor_for(k, o, fr, fc);
            if (a >= 0 && block_fits(&puzzle, &board, k, o, a)) {
                board.orient[k] = (uint8_t)o;
                board.anchor[k] = (uint8_t)a;
                pref[k] = (uint8_t)o;
                ok = true;
            }
        }
        board.placed[k] = 1;
        res.changed = ok;
        return res;
    }
    if (cur < 0) return res;
    if (try_place(cur, r, c, pref[cur])) {
        res.changed = true;
        cur = next_unplaced(cur);
        res.solved = block_solved(&puzzle, &board);
    }
    return res;
}

static bool block_is_solved(void) { return block_solved(&puzzle, &board); }

static int block_hint(int *r, int *c)
{
    int piece, orient, anchor;
    if (!block_next_step(&puzzle, &board, &piece, &orient, &anchor)) {
        if (piece < 0) return HINT_NOTHING;
        int cells[BLOCK_MAX_SIZE];
        block_piece_cells(&puzzle, piece, board.orient[piece], board.anchor[piece], cells);
        *r = cells[0] / puzzle.n;
        *c = cells[0] % puzzle.n;
        return HINT_WRONG_PLACEMENT;
    }
    board.placed[piece] = 1;
    board.orient[piece] = (uint8_t)orient;
    board.anchor[piece] = (uint8_t)anchor;
    if (cur == piece || cur < 0) cur = next_unplaced(piece);
    int cells[BLOCK_MAX_SIZE];
    block_piece_cells(&puzzle, piece, orient, anchor, cells);
    *r = cells[0] / puzzle.n;
    *c = cells[0] % puzzle.n;
    return HINT_APPLIED;
}

static void block_aux(void)
{
    if (cur >= 0) cur = next_unplaced(cur);
}

static int block_tray(uint8_t *rc, int max)
{
    if (cur < 0) return 0;
    BlockShape s;
    block_shape_get(puzzle.shape[cur], pref[cur], &s);
    int k = 0;
    for (int i = 0; i < s.size && k + 1 < max; i++) { rc[k++] = s.r[i]; rc[k++] = s.c[i]; }
    return s.size;
}

static int block_save(uint8_t *buf)
{
    buf[0] = puzzle.count;
    for (int k = 0; k < puzzle.count; k++) {
        buf[1 + 3 * k] = board.placed[k];
        buf[2 + 3 * k] = board.orient[k];
        buf[3 + 3 * k] = board.anchor[k];
    }
    return 1 + 3 * puzzle.count;
}

static bool block_restore(const uint8_t *buf, int len)
{
    if (len < 1 || buf[0] != puzzle.count || len < 1 + 3 * puzzle.count) return false;
    BlockBoard b;
    block_board_init(&b);
    for (int k = 0; k < puzzle.count; k++) {
        if (!buf[1 + 3 * k]) continue;
        int o = buf[2 + 3 * k], a = buf[3 + 3 * k];
        if (o >= block_shape_orients(puzzle.shape[k]) || !block_fits(&puzzle, &b, k, o, a)) return false;
        b.placed[k] = 1;
        b.orient[k] = (uint8_t)o;
        b.anchor[k] = (uint8_t)a;
    }
    board = b;
    cur = next_unplaced(puzzle.count - 1);
    return true;
}

const PuzzleOps ops_block = {
    .family = FAM_BLOCK,
    .name_str = STR_ROOM_BLOCK,
    .help_str = STR_HELP_BLOCK,
    .key_a_str = STR_KEY_A_PLACE,
    .key_b_str = STR_KEY_B_TAKE,
    .load = block_load,
    .size = block_size,
    .cell = block_cell,
    .action = block_action,
    .solved = block_is_solved,
    .hint = block_hint,
    .save = block_save,
    .restore = block_restore,
    .aux = block_aux,
    .tray = block_tray,
    .key_r_str = STR_KEY_R_NEXT_BLOCK,
};
