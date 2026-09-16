// HEART adapter: the core's picture, on 8 px cells.
//   A: mark ore (again: clear)      B: note rock (again: clear)
// Ore marked where the picture has rock is a conflict: the room blinks it and
// charges stability after a couple of seconds, like the other families. Rock
// notes are never wrong by themselves. Given cells are fixed.
#include "puzzle.h"
#include "render.h"
#include "lang.h"
#include "heart.h"

static HeartPuzzle puzzle;
static HeartBoard  board;
static uint8_t     conflict[HEART_MAX_CELLS];

// small cell tiles (assets/cells_small.png)
enum { SC_UNKNOWN = 0, SC_ORE, SC_ROCK, SC_FLAT, SC_ORE_LIT };

static void refresh_conflicts(void) { heart_conflicts(&puzzle, &board, conflict); } 

static bool heart_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_HEART || !heart_unpack(payload, h->size, &puzzle)) return false;
    heart_board_init(&puzzle, &board);
    memset(conflict, 0, sizeof conflict);
    return true;
}

static int heart_size(void) { return puzzle.n; }

static void heart_cell(int r, int c, CellView *out)
{
    int i = cell_at(puzzle.n, r, c);
    out->edges = 0;
    out->mark = MARK_NONE;
    out->conflict = conflict[i];
    switch (board.cell[i]) {
    case HEART_ORE:  out->variant = SC_ORE;  out->pal = (u8)(conflict[i] ? PAL_CELL_CONFLICT : PAL_CELL_HILITE); break;
    case HEART_ROCK: out->variant = SC_ROCK; out->pal = PAL_REGION0 + 6; break;
    default:         out->variant = SC_UNKNOWN; out->pal = PAL_REGION0; break;
    }
}

static ActionResult heart_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int i = cell_at(puzzle.n, r, c);
    if (puzzle.given[i]) return res;
    uint8_t before = board.cell[i];
    if (act == ACT_A) board.cell[i] = before == HEART_ORE ? HEART_UNKNOWN : HEART_ORE;
    else              board.cell[i] = before == HEART_ROCK ? HEART_UNKNOWN : HEART_ROCK;
    res.changed = board.cell[i] != before;
    refresh_conflicts();
    res.mistake = board.cell[i] == HEART_ORE && conflict[i];
    res.solved = heart_solved(&puzzle, &board);
    return res;
}

static bool heart_is_solved(void) { return heart_solved(&puzzle, &board); }

// A hint points at a wrong ore mark first; otherwise it sounds the line with
// the fewest cells left (finishing a line feels best) and reveals up to five.
#define HINT_CELLS 5
static int heart_hint(int *r, int *c)
{
    int n = puzzle.n;
    for (int i = 0; i < n * n; i++)
        if (conflict[i]) { *r = i / n; *c = i % n; return HINT_WRONG_PLACEMENT; }
    if (heart_solved(&puzzle, &board)) return HINT_NOTHING;
    int best = -1, best_left = n + 1;
    for (int line = 0; line < 2 * n; line++) {
        int left = 0;
        for (int k = 0; k < n; k++) {
            int i = line < n ? cell_at(n, line, k) : cell_at(n, k, line - n);
            left += (board.cell[i] == HEART_ORE) != (puzzle.picture[i] != 0) || board.cell[i] == HEART_UNKNOWN;
        }
        if (left && left < best_left) { best_left = left; best = line; }
    }
    if (best < 0) return HINT_NOTHING;
    int revealed = 0, first = -1;
    for (int k = 0; k < n && revealed < HINT_CELLS; k++) {
        int i = best < n ? cell_at(n, best, k) : cell_at(n, k, best - n);
        uint8_t want = puzzle.picture[i] ? HEART_ORE : HEART_ROCK;
        if (board.cell[i] == want) continue;
        board.cell[i] = want;
        if (first < 0) first = i;
        revealed++;
    }
    refresh_conflicts();
    *r = first / n;
    *c = first % n;
    return HINT_APPLIED;
}

static int heart_save(uint8_t *buf)
{
    int cells = puzzle.n * puzzle.n, len = (cells + 3) / 4;
    memset(buf, 0, len);
    for (int i = 0; i < cells; i++) buf[i >> 2] |= (uint8_t)((board.cell[i] & 3) << ((i & 3) * 2));
    return len;
}

static bool heart_restore(const uint8_t *buf, int len)
{
    int cells = puzzle.n * puzzle.n;
    if (len != (cells + 3) / 4) return false;
    for (int i = 0; i < cells; i++) {
        uint8_t v = (uint8_t)((buf[i >> 2] >> ((i & 3) * 2)) & 3);
        if (v > HEART_ROCK) return false;
        board.cell[i] = puzzle.given[i] ? (puzzle.picture[i] ? HEART_ORE : HEART_ROCK) : v;
    }
    refresh_conflicts();
    return true;
}

static int heart_clues(int line, uint8_t *out, bool *done)
{
    *done = heart_line_done(&puzzle, &board, line);
    return heart_line_clues(&puzzle, line, out);
}

const PuzzleOps ops_heart = {
    .family = FAM_HEART,
    .name_str = STR_ROOM_HEART,
    .help_str = STR_HELP_HEART,
    .key_a_str = STR_KEY_A_MINE,
    .key_b_str = STR_KEY_B_ROCK,
    .load = heart_load,
    .size = heart_size,
    .cell = heart_cell,
    .action = heart_action,
    .solved = heart_is_solved,
    .hint = heart_hint,
    .save = heart_save,
    .restore = heart_restore,
    .small_cells = true,
    .line_clues = heart_clues,
    .time_scale = 300,
};
