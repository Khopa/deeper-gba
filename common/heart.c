#include "heart.h"

const uint8_t heart_sizes[HEART_SIZE_COUNT] = { 10, 12, 15 };

int heart_difficulty(int n)
{
    return n <= 10 ? 4 : n <= 12 ? 7 : 10;
}

int heart_col_clue_rows(int n)
{
    // header row + clue rows + n rows of 8 px must fit the 20 screen rows
    return n > 12 ? 4 : 5;
}

// --- rules --------------------------------------------------------------------------

void heart_board_init(const HeartPuzzle *p, HeartBoard *b)
{
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++)
        b->cell[i] = p->given[i] ? (p->picture[i] ? HEART_ORE : HEART_ROCK) : HEART_UNKNOWN;
}

// cell index of the k-th cell along a line
static int line_cell(int n, int line, int k)
{
    return line < n ? cell_at(n, line, k) : cell_at(n, k, line - n);
}

static int runs_of(int n, const uint8_t *v, int stride_line, int line, uint8_t *out, bool ore_only)
{
    int count = 0, run = 0;
    (void)stride_line;
    for (int k = 0; k < n; k++) {
        int i = line_cell(n, line, k);
        bool ore = ore_only ? v[i] == HEART_ORE : v[i] != 0;
        if (ore) run++;
        else if (run) { out[count++] = (uint8_t)run; run = 0; }
    }
    if (run) out[count++] = (uint8_t)run;
    if (!count) out[count++] = 0;
    return count;
}

int heart_line_clues(const HeartPuzzle *p, int line, uint8_t *out)
{
    return runs_of(p->n, p->picture, 0, line, out, false);
}

bool heart_line_done(const HeartPuzzle *p, const HeartBoard *b, int line)
{
    uint8_t want[HEART_MAX_N], have[HEART_MAX_N];
    int nw = heart_line_clues(p, line, want);
    int nh = runs_of(p->n, b->cell, 0, line, have, true);
    if (nw != nh) return false;
    for (int k = 0; k < nw; k++) if (want[k] != have[k]) return false;
    return true;
}

int heart_conflicts(const HeartPuzzle *p, const HeartBoard *b, uint8_t *conflict)
{
    int cells = p->n * p->n, count = 0;
    for (int i = 0; i < cells; i++) {
        bool bad = b->cell[i] == HEART_ORE && !p->picture[i];
        if (conflict) conflict[i] = (uint8_t)bad;
        count += bad;
    }
    return count;
}

bool heart_solved(const HeartPuzzle *p, const HeartBoard *b)
{
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++)
        if ((b->cell[i] == HEART_ORE) != (p->picture[i] != 0)) return false;
    return true;
}

bool heart_fits_layout(const HeartPuzzle *p)
{
    int n = p->n;
    uint8_t clues[HEART_MAX_N];
    for (int line = 0; line < 2 * n; line++) {
        int count = heart_line_clues(p, line, clues);
        int chars = count - 1, rows = 0;                  // separators / stacked digits
        for (int k = 0; k < count; k++) {
            chars += clues[k] >= 10 ? 2 : 1;
            rows += clues[k] >= 10 ? 2 : 1;
        }
        if (line < n && chars > HEART_ROW_CLUE_CHARS) return false;
        if (line >= n && rows > heart_col_clue_rows(n)) return false;
    }
    return true;
}

// --- packing -----------------------------------------------------------------------

int heart_payload_len(int n) { return 2 * ((n * n + 7) / 8); }

static void pack_bits(const uint8_t *v, int count, uint8_t *out)
{
    int bytes = (count + 7) / 8;
    memset(out, 0, bytes);
    for (int i = 0; i < count; i++) if (v[i]) out[i >> 3] |= (uint8_t)(1 << (i & 7));
}

static void unpack_bits(const uint8_t *in, int count, uint8_t *v)
{
    for (int i = 0; i < count; i++) v[i] = (uint8_t)((in[i >> 3] >> (i & 7)) & 1);
}

int heart_pack(const HeartPuzzle *p, uint8_t *out)
{
    int cells = p->n * p->n, bytes = (cells + 7) / 8;
    pack_bits(p->picture, cells, out);
    pack_bits(p->given, cells, out + bytes);
    return 2 * bytes;
}

bool heart_unpack(const uint8_t *in, int n, HeartPuzzle *p)
{
    if (n < HEART_MIN_N || n > HEART_MAX_N) return false;
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)n;
    int cells = n * n, bytes = (cells + 7) / 8;
    unpack_bits(in, cells, p->picture);
    unpack_bits(in + bytes, cells, p->given);
    return true;
}

// --- line solver ------------------------------------------------------------------------
// Enumerate every placement of the runs compatible with the known cells and
// note, per cell, whether some placement leaves it ore and whether some
// leaves it rock. Lines are short (n <= 15) and runs few, so plain recursion
// with pruning is fast enough even for the greedy given picker.

typedef struct {
    int n, count;
    const uint8_t *clues;
    const uint8_t *cells;
    uint8_t can_ore[HEART_MAX_N], can_rock[HEART_MAX_N];
    uint8_t place[HEART_MAX_N];              // current placement (1 = ore)
    long placements;
} LineCtx;

static void place_rec(LineCtx *lc, int run, int pos)
{
    int n = lc->n;
    if (run == lc->count) {
        for (int k = pos; k < n; k++) {         // the tail must be rock
            if (lc->cells[k] == HEART_ORE) return;
            lc->place[k] = 0;
        }
        lc->placements++;
        for (int k = 0; k < n; k++) {
            if (lc->place[k]) lc->can_ore[k] = 1; else lc->can_rock[k] = 1;
        }
        return;
    }
    int len = lc->clues[run];
    // room needed for this run and the ones after it (one gap each)
    int need = len;
    for (int r = run + 1; r < lc->count; r++) need += 1 + lc->clues[r];
    for (int s = pos; s + need <= n; s++) {
        if (s > pos && lc->cells[s - 1] == HEART_ORE) break;   // a known ore cell before the run: must be covered
        bool ok = true;
        for (int k = s; k < s + len; k++) if (lc->cells[k] == HEART_ROCK) { ok = false; break; }
        if (!ok) continue;
        if (s + len < n && lc->cells[s + len] == HEART_ORE) continue;   // the run would touch ore after it
        for (int k = pos; k < s; k++) lc->place[k] = 0;
        for (int k = s; k < s + len; k++) lc->place[k] = 1;
        int next = s + len;
        if (next < n) { lc->place[next] = 0; next++; }
        place_rec(lc, run + 1, next);
    }
}

bool heart_line_solve(int n, const uint8_t *clues, int count, uint8_t *cells)
{
    LineCtx lc;
    memset(&lc, 0, sizeof lc);
    lc.n = n;
    lc.clues = clues;
    lc.cells = cells;
    lc.count = (count == 1 && clues[0] == 0) ? 0 : count;
    place_rec(&lc, 0, 0);
    if (!lc.placements) return false;
    for (int k = 0; k < n; k++) {
        if (cells[k] != HEART_UNKNOWN) continue;
        if (lc.can_ore[k] && !lc.can_rock[k]) cells[k] = HEART_ORE;
        else if (lc.can_rock[k] && !lc.can_ore[k]) cells[k] = HEART_ROCK;
    }
    return true;
}

bool heart_deduce(const HeartPuzzle *p, HeartBoard *b, int *passes)
{
    int n = p->n, sweeps = 0;
    bool progress = true;
    while (progress) {
        progress = false;
        sweeps++;
        for (int line = 0; line < 2 * n; line++) {
            uint8_t clues[HEART_MAX_N], cells[HEART_MAX_N];
            int count = heart_line_clues(p, line, clues);
            for (int k = 0; k < n; k++) cells[k] = b->cell[line_cell(n, line, k)];
            if (!heart_line_solve(n, clues, count, cells)) { if (passes) *passes = sweeps; return false; }
            for (int k = 0; k < n; k++) {
                int i = line_cell(n, line, k);
                if (b->cell[i] != cells[k]) { b->cell[i] = cells[k]; progress = true; }
            }
        }
    }
    if (passes) *passes = sweeps;
    for (int i = 0; i < n * n; i++) if (b->cell[i] == HEART_UNKNOWN) return false;
    return true;
}
