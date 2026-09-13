// BLOCK adapter: fit the stone blocks into the cavity.
//   A: place the current block with its bounding box's top-left corner on the
//      cursor (a ghost outline shows where it would go and whether it fits);
//      on a placed block, when nothing can be placed there: take it back
//   B: choose the next block still to place (shown in the tray)
//   R: turn the current block (every orientation, mirrored ones included)
// Blocks never conflict: a placement that does not fit is simply refused.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "block.h"

static BlockPuzzle puzzle;
static BlockBoard  board;
// Not static: the emulator scenarios read them from RAM.
int     block_cur;                            // piece selected for placement, -1 when all placed
uint8_t block_orient[BLOCK_MAX_PIECES];       // orientation chosen per piece
static int8_t owner[PUZZLE_MAX_CELLS];         // piece covering each cell, -1 if none (cache for cell views)

static void refresh_owner(void)
{
    int cells = puzzle.n * puzzle.n;
    for (int i = 0; i < cells; i++) owner[i] = -1;
    for (int k = 0; k < puzzle.count; k++) {
        if (!board.placed[k]) continue;
        int c[BLOCK_MAX_SIZE];
        int m = block_piece_cells(&puzzle, k, board.orient[k], board.anchor[k], c);
        for (int i = 0; i < m; i++) owner[c[i]] = (int8_t)k;
    }
}

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
    memset(block_orient, 0, sizeof block_orient);
    block_cur = 0;
    refresh_owner();
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
    int k = owner[i];
    if (k < 0) {
        out->variant = 1;                        // empty cavity: a dark hole
        out->edges = 0;
        out->pal = PAL_REGION0 + 0;
        return;
    }
    int edges = 0;
    if (r == 0 || owner[cell_at(n, r - 1, c)] != k) edges |= 1;
    if (c == n - 1 || owner[cell_at(n, r, c + 1)] != k) edges |= 2;
    if (r == n - 1 || owner[cell_at(n, r + 1, c)] != k) edges |= 4;
    if (c == 0 || owner[cell_at(n, r, c - 1)] != k) edges |= 8;
    out->variant = 0;
    out->edges = (u8)edges;
    out->pal = (u8)(PAL_REGION0 + 1 + (k % 7));
}

static ActionResult block_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int n = puzzle.n, i = cell_at(n, r, c);
    if (act == ACT_B) {                          // next block to place
        if (block_cur >= 0) {
            int next = next_unplaced(block_cur);
            res.changed = next != block_cur;
            block_cur = next;
        }
        return res;
    }
    if (block_cur >= 0 && block_fits(&puzzle, &board, block_cur, block_orient[block_cur], i)) {
        board.placed[block_cur] = 1;
        board.orient[block_cur] = block_orient[block_cur];
        board.anchor[block_cur] = (uint8_t)i;
        block_cur = next_unplaced(block_cur);
        refresh_owner();
        res.changed = true;
        res.solved = block_solved(&puzzle, &board);
        return res;
    }
    int k = owner[i];
    if (k >= 0) {                                // take a placed block back
        board.placed[k] = 0;
        block_orient[k] = board.orient[k];
        block_cur = k;
        refresh_owner();
        res.changed = true;
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
    if (block_cur == piece || block_cur < 0) block_cur = next_unplaced(piece);
    refresh_owner();
    *r = anchor / puzzle.n;
    *c = anchor % puzzle.n;
    return HINT_APPLIED;
}

static void block_aux(void)                       // R: turn the current block
{
    if (block_cur < 0) return;
    int no = block_shape_orients(puzzle.shape[block_cur]);
    block_orient[block_cur] = (uint8_t)((block_orient[block_cur] + 1) % no);
}

static int block_tray(uint8_t *rc, int max)
{
    if (block_cur < 0) return 0;
    BlockShape s;
    block_shape_get(puzzle.shape[block_cur], block_orient[block_cur], &s);
    int k = 0;
    for (int i = 0; i < s.size && k + 1 < max; i++) { rc[k++] = s.r[i]; rc[k++] = s.c[i]; }
    return s.size;
}

static int block_ghost(int r, int c, uint8_t *cells, bool *fits)
{
    if (block_cur < 0) return 0;
    BlockShape s;
    block_shape_get(puzzle.shape[block_cur], block_orient[block_cur], &s);
    int n = puzzle.n, k = 0;
    *fits = block_fits(&puzzle, &board, block_cur, block_orient[block_cur], cell_at(n, r, c));
    for (int i = 0; i < s.size; i++) {
        int rr = r + s.r[i], cc = c + s.c[i];
        if (rr >= n || cc >= n) continue;          // the part that sticks out is not drawn
        cells[2 * k] = (uint8_t)rr;
        cells[2 * k + 1] = (uint8_t)cc;
        k++;
    }
    return k;
}

static int block_save(uint8_t *buf)
{
    buf[0] = puzzle.count;
    for (int k = 0; k < puzzle.count; k++) {
        buf[1 + 3 * k] = board.placed[k];
        buf[2 + 3 * k] = board.placed[k] ? board.orient[k] : block_orient[k];
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
        int o = buf[2 + 3 * k], a = buf[3 + 3 * k];
        if (o >= block_shape_orients(puzzle.shape[k])) return false;
        block_orient[k] = (uint8_t)o;
        if (!buf[1 + 3 * k]) continue;
        if (!block_fits(&puzzle, &b, k, o, a)) return false;
        b.placed[k] = 1;
        b.orient[k] = (uint8_t)o;
        b.anchor[k] = (uint8_t)a;
    }
    board = b;
    block_cur = next_unplaced(puzzle.count - 1);
    refresh_owner();
    return true;
}

const PuzzleOps ops_block = {
    .family = FAM_BLOCK,
    .name_str = STR_ROOM_BLOCK,
    .help_str = STR_HELP_BLOCK,
    .key_a_str = STR_KEY_A_PLACE,
    .key_b_str = STR_KEY_B_NEXT_BLOCK,
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
    .key_r_str = STR_KEY_R_TURN,
    .ghost = block_ghost,
};
