// DIG rules, packing and human-style deduction solver. See dig.h.
#include "dig.h"

// --- geometry helpers ----------------------------------------------------------

typedef struct {
    int      n;
    uint64_t all;                       // every cell
    uint64_t row[PUZZLE_MAX_N];
    uint64_t col[PUZZLE_MAX_N];
    uint64_t reg[PUZZLE_MAX_N];
    uint64_t nbr[PUZZLE_MAX_CELLS];     // the 8 neighbours of a cell (in-grid)
} DigGeom;

static void geom_init(DigGeom *g, const DigPuzzle *p)
{
    int n = p->n;
    memset(g, 0, sizeof *g);
    g->n = n;
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            int i = cell_at(n, r, c);
            uint64_t b = bit64(i);
            g->all |= b;
            g->row[r] |= b;
            g->col[c] |= b;
            g->reg[p->region[i]] |= b;
            for (int dr = -1; dr <= 1; dr++)
                for (int dc = -1; dc <= 1; dc++) {
                    if (!dr && !dc) continue;
                    int rr = r + dr, cc = c + dc;
                    if (rr >= 0 && rr < n && cc >= 0 && cc < n)
                        g->nbr[i] |= bit64(cell_at(n, rr, cc));
                }
        }
}

// Everything a dig at `i` excludes (its row, column, region, neighbours), minus itself.
static uint64_t excluded_by(const DigGeom *g, const DigPuzzle *p, int i)
{
    int r = i / g->n, c = i % g->n;
    return (g->row[r] | g->col[c] | g->reg[p->region[i]] | g->nbr[i]) & ~bit64(i);
}

// --- rules -----------------------------------------------------------------------

int dig_count_digs(const DigBoard *b)
{
    int n = 0;
    for (int i = 0; i < PUZZLE_MAX_CELLS; i++) n += b->cell[i] == DIG_DIG;
    return n;
}

int dig_conflicts(const DigPuzzle *p, const DigBoard *b, uint8_t *conflict)
{
    int n = p->n, cells = n * n;
    int row_cnt[PUZZLE_MAX_N] = {0}, col_cnt[PUZZLE_MAX_N] = {0}, reg_cnt[PUZZLE_MAX_N] = {0};
    if (conflict) memset(conflict, 0, cells);

    for (int i = 0; i < cells; i++)
        if (b->cell[i] == DIG_DIG) {
            row_cnt[i / n]++;
            col_cnt[i % n]++;
            reg_cnt[p->region[i]]++;
        }

    int bad = 0;
    for (int i = 0; i < cells; i++) {
        if (b->cell[i] != DIG_DIG) continue;
        int r = i / n, c = i % n;
        bool conflicted = row_cnt[r] > 1 || col_cnt[c] > 1 || reg_cnt[p->region[i]] > 1;
        for (int dr = -1; dr <= 1 && !conflicted; dr++)
            for (int dc = -1; dc <= 1; dc++) {
                if (!dr && !dc) continue;
                int rr = r + dr, cc = c + dc;
                if (rr >= 0 && rr < n && cc >= 0 && cc < n && b->cell[cell_at(n, rr, cc)] == DIG_DIG) {
                    conflicted = true;
                    break;
                }
            }
        if (conflicted) {
            bad++;
            if (conflict) conflict[i] = 1;
        }
    }
    return bad;
}

bool dig_solved(const DigPuzzle *p, const DigBoard *b)
{
    int n = p->n;
    for (int r = 0; r < n; r++) {
        int cnt = 0;
        for (int c = 0; c < n; c++) cnt += b->cell[cell_at(n, r, c)] == DIG_DIG;
        if (cnt != 1) return false;
    }
    return dig_conflicts(p, b, NULL) == 0;
}

// --- packing -------------------------------------------------------------------

int dig_payload_len(int n)
{
    return (n * n + 1) / 2 + (n + 1) / 2;
}

static void put_nibble(uint8_t *out, int idx, int v)
{
    if (idx & 1) out[idx >> 1] |= (uint8_t)(v << 4);
    else         out[idx >> 1]  = (uint8_t)(v & 15);
}

static int get_nibble(const uint8_t *in, int idx)
{
    return (idx & 1) ? in[idx >> 1] >> 4 : in[idx >> 1] & 15;
}

int dig_pack(const DigPuzzle *p, uint8_t *out)
{
    int n = p->n, cells = n * n;
    int len = dig_payload_len(n);
    memset(out, 0, len);
    for (int i = 0; i < cells; i++) put_nibble(out, i, p->region[i]);
    uint8_t *sol = out + (cells + 1) / 2;
    for (int r = 0; r < n; r++) put_nibble(sol, r, p->solution[r]);
    return len;
}

bool dig_unpack(const uint8_t *in, int n, DigPuzzle *p)
{
    if (n < DIG_MIN_N || n > DIG_MAX_N) return false;
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)n;
    int cells = n * n;
    for (int i = 0; i < cells; i++) {
        int v = get_nibble(in, i);
        if (v >= n) return false;
        p->region[i] = (uint8_t)v;
    }
    const uint8_t *sol = in + (cells + 1) / 2;
    for (int r = 0; r < n; r++) {
        int v = get_nibble(sol, r);
        if (v >= n) return false;
        p->solution[r] = (uint8_t)v;
    }
    return true;
}

// --- deduction solver ------------------------------------------------------------
//
// State: `cand` = cells that may still receive a dig, `placed` = digs.
// A unit (row, column or region) is satisfied when it holds a placed dig.

typedef struct {
    const DigGeom   *g;
    const DigPuzzle *p;
    uint64_t cand, placed;
    int      n_placed;
    bool     contradiction;
} DigState;

static void place(DigState *s, int i)
{
    s->placed |= bit64(i);
    s->n_placed++;
    s->cand &= ~(excluded_by(s->g, s->p, i) | bit64(i));
}

static bool unit_satisfied(const DigState *s, uint64_t unit) { return (s->placed & unit) != 0; }

// Which rows/cols/regions does a mask span? Returned as small bitmasks.
static int rows_of(const DigGeom *g, uint64_t m)
{
    int bits = 0;
    for (int r = 0; r < g->n; r++) if (m & g->row[r]) bits |= 1 << r;
    return bits;
}
static int cols_of(const DigGeom *g, uint64_t m)
{
    int bits = 0;
    for (int c = 0; c < g->n; c++) if (m & g->col[c]) bits |= 1 << c;
    return bits;
}
static int regs_of(const DigGeom *g, uint64_t m)
{
    int bits = 0;
    for (int k = 0; k < g->n; k++) if (m & g->reg[k]) bits |= 1 << k;
    return bits;
}
static int popcount32(int x) { int n = 0; while (x) { x &= x - 1; n++; } return n; }
static int ctz32(int x) { int n = 0; while (!(x & 1)) { x >>= 1; n++; } return n; }

// Technique 0: a unit with a single candidate forces a dig; an empty
// unsatisfied unit is a contradiction. Returns the placed cell or -1.
static int tech_single(DigState *s)
{
    const DigGeom *g = s->g;
    const uint64_t *units[3] = { g->row, g->col, g->reg };
    for (int u = 0; u < 3; u++)
        for (int k = 0; k < g->n; k++) {
            uint64_t unit = units[u][k];
            if (unit_satisfied(s, unit)) continue;
            uint64_t c = s->cand & unit;
            if (!c) { s->contradiction = true; return -1; }
            if (popcount64(c) == 1) {
                int i = ctz64(c);
                place(s, i);
                return i;
            }
        }
    return -1;
}

// Eliminate `m` from the candidates; returns the mask actually removed.
static uint64_t eliminate(DigState *s, uint64_t m)
{
    uint64_t removed = s->cand & m;
    s->cand &= ~m;
    return removed;
}

// Technique 1: confinement (line/region interactions). Returns removed mask.
static uint64_t tech_confine(DigState *s)
{
    const DigGeom *g = s->g;
    for (int k = 0; k < g->n; k++) {
        uint64_t rc = s->cand & g->reg[k];
        if (!rc || unit_satisfied(s, g->reg[k])) continue;
        int rows = rows_of(g, rc), cols = cols_of(g, rc);
        if (popcount32(rows) == 1) {
            uint64_t rem = eliminate(s, g->row[ctz32(rows)] & ~g->reg[k]);
            if (rem) return rem;
        }
        if (popcount32(cols) == 1) {
            uint64_t rem = eliminate(s, g->col[ctz32(cols)] & ~g->reg[k]);
            if (rem) return rem;
        }
    }
    for (int k = 0; k < g->n; k++) {
        uint64_t lines[2] = { s->cand & g->row[k], s->cand & g->col[k] };
        uint64_t whole[2] = { g->row[k], g->col[k] };
        for (int l = 0; l < 2; l++) {
            if (!lines[l] || unit_satisfied(s, whole[l])) continue;
            int regs = regs_of(g, lines[l]);
            if (popcount32(regs) == 1) {
                uint64_t rem = eliminate(s, g->reg[ctz32(regs)] & ~whole[l]);
                if (rem) return rem;
            }
        }
    }
    return 0;
}

// Technique 2: k units of one kind confined to k units of another kind
// (k = 2 or 3): the other cells of those k target units are eliminated.
static uint64_t tech_pairs(DigState *s)
{
    const DigGeom *g = s->g;
    int n = g->n;
    // (source units, target units, span function) in the four useful directions
    struct { const uint64_t *src, *dst; int (*span)(const DigGeom *, uint64_t); } dirs[4] = {
        { g->reg, g->row, rows_of }, { g->reg, g->col, cols_of },
        { g->row, g->reg, regs_of }, { g->col, g->reg, regs_of },
    };
    for (int d = 0; d < 4; d++) {
        int span[PUZZLE_MAX_N];
        for (int k = 0; k < n; k++)
            span[k] = unit_satisfied(s, dirs[d].src[k]) ? -1 : dirs[d].span(g, s->cand & dirs[d].src[k]);
        for (int size = 2; size <= 3; size++)
            for (int a = 0; a < n; a++) {
                if (span[a] < 0 || popcount32(span[a]) > size) continue;
                for (int b = a + 1; b < n; b++) {
                    if (span[b] < 0) continue;
                    int ab = span[a] | span[b];
                    if (popcount32(ab) > size) continue;
                    if (size == 2) {
                        if (popcount32(ab) != 2) continue;
                        uint64_t srcs = dirs[d].src[a] | dirs[d].src[b], dsts = 0;
                        for (int t = 0; t < n; t++) if (ab & (1 << t)) dsts |= dirs[d].dst[t];
                        uint64_t rem = eliminate(s, dsts & ~srcs);
                        if (rem) return rem;
                    } else {
                        for (int c = b + 1; c < n; c++) {
                            if (span[c] < 0) continue;
                            int abc = ab | span[c];
                            if (popcount32(abc) != 3) continue;
                            uint64_t srcs = dirs[d].src[a] | dirs[d].src[b] | dirs[d].src[c], dsts = 0;
                            for (int t = 0; t < n; t++) if (abc & (1 << t)) dsts |= dirs[d].dst[t];
                            uint64_t rem = eliminate(s, dsts & ~srcs);
                            if (rem) return rem;
                        }
                    }
                }
            }
    }
    return 0;
}

// Technique 3: one-step trial. A candidate whose placement would leave some
// unsatisfied unit without candidates cannot be a dig.
static uint64_t tech_trial(DigState *s)
{
    const DigGeom *g = s->g;
    uint64_t c = s->cand;
    while (c) {
        int i = ctz64(c);
        c &= c - 1;
        uint64_t after = s->cand & ~(excluded_by(g, s->p, i) | bit64(i));
        uint64_t sat = s->placed | bit64(i);
        const uint64_t *units[3] = { g->row, g->col, g->reg };
        for (int u = 0; u < 3; u++)
            for (int k = 0; k < g->n; k++) {
                uint64_t unit = units[u][k];
                if ((sat & unit) || (after & unit)) continue;
                return eliminate(s, bit64(i));
            }
    }
    return 0;
}

static void state_from_board(DigState *s, const DigGeom *g, const DigPuzzle *p, const DigBoard *b)
{
    memset(s, 0, sizeof *s);
    s->g = g;
    s->p = p;
    s->cand = g->all;
    if (!b) return;
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++)
        if (b->cell[i] == DIG_DIG) place(s, i);
    for (int i = 0; i < cells; i++)
        if (b->cell[i] == DIG_CROSS) s->cand &= ~bit64(i);
}

// Apply the cheapest technique that makes progress. Returns the technique
// index (or -1 when stuck / contradictory) and reports the change.
static int deduce_step(DigState *s, int *changed_cell, int *changed_kind)
{
    int i = tech_single(s);
    if (s->contradiction) return -1;
    if (i >= 0) { *changed_cell = i; *changed_kind = DIG_DIG; return DIGT_SINGLE; }

    uint64_t rem;
    if ((rem = tech_confine(s))) { *changed_cell = ctz64(rem); *changed_kind = DIG_CROSS; return DIGT_CONFINE; }
    if ((rem = tech_pairs(s)))   { *changed_cell = ctz64(rem); *changed_kind = DIG_CROSS; return DIGT_PAIRS; }
    if ((rem = tech_trial(s)))   { *changed_cell = ctz64(rem); *changed_kind = DIG_CROSS; return DIGT_TRIAL; }
    return -1;
}

void dig_deduce(const DigPuzzle *p, const DigBoard *start, DigSolveStats *st)
{
    DigGeom g;
    DigState s;
    geom_init(&g, p);
    state_from_board(&s, &g, p, start);
    memset(st, 0, sizeof *st);
    st->hardest = -1;

    while (s.n_placed < p->n) {
        int cell, kind;
        int t = deduce_step(&s, &cell, &kind);
        if (t < 0) break;
        st->steps[t]++;
        if (t > st->hardest) st->hardest = t;
    }
    st->solved = s.n_placed == p->n && !s.contradiction;
    st->candidates = s.cand;
    st->placed = s.placed;
}

bool dig_next_step(const DigPuzzle *p, const DigBoard *b, int *cell, int *kind)
{
    int n = p->n, cells = n * n;
    // Hints only make sense on a board whose digs are all correct.
    for (int i = 0; i < cells; i++)
        if (b->cell[i] == DIG_DIG && p->solution[i / n] != i % n) return false;

    DigGeom g;
    DigState s;
    geom_init(&g, p);
    state_from_board(&s, &g, p, b);
    if (s.n_placed == n) return false;

    int c, k;
    int t = deduce_step(&s, &c, &k);
    if (t < 0) return false;
    *cell = c;
    *kind = k;
    return true;
}

// --- backtracking counter ----------------------------------------------------------

typedef struct {
    const DigPuzzle *p;
    int n, cap, found;
    uint8_t col_used[PUZZLE_MAX_N], reg_used[PUZZLE_MAX_N];
    int prev_col;
} DigCount;

static void count_rec(DigCount *k, int r)
{
    if (k->found >= k->cap) return;
    if (r == k->n) { k->found++; return; }
    for (int c = 0; c < k->n; c++) {
        if (k->col_used[c]) continue;
        if (r > 0 && (c == k->prev_col - 1 || c == k->prev_col + 1)) continue;
        int reg = k->p->region[cell_at(k->n, r, c)];
        if (k->reg_used[reg]) continue;
        int saved = k->prev_col;
        k->col_used[c] = k->reg_used[reg] = 1;
        k->prev_col = c;
        count_rec(k, r + 1);
        k->col_used[c] = k->reg_used[reg] = 0;
        k->prev_col = saved;
    }
}

int dig_count_solutions(const DigPuzzle *p, int cap)
{
    DigCount k;
    memset(&k, 0, sizeof k);
    k.p = p;
    k.n = p->n;
    k.cap = cap;
    k.prev_col = -5;
    count_rec(&k, 0);
    return k.found;
}

// --- difficulty ----------------------------------------------------------------------

int dig_difficulty(const DigPuzzle *p, const DigSolveStats *st)
{
    if (!st->solved) return 0;
    // Work score: singles are free, confinements cheap, pairs and trials
    // expensive; then a mild bonus for the larger grids.
    int score = st->steps[DIGT_CONFINE] + 3 * st->steps[DIGT_PAIRS] + 3 * st->steps[DIGT_TRIAL];
    static const int level_for_score[] = { 0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 8 };
    int level = score < (int)(sizeof level_for_score / sizeof level_for_score[0])
                    ? level_for_score[score] : 9;
    int d = 1 + level + (p->n >= 7 ? 1 : 0);
    if (st->steps[DIGT_PAIRS] && d < 4) d = 4;
    if (st->steps[DIGT_TRIAL] && d < 5) d = 5;
    if (d < DIFF_MIN) d = DIFF_MIN;
    if (d > DIFF_MAX) d = DIFF_MAX;
    return d;
}
