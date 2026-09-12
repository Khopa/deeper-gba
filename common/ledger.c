// LEDGER rules, packing and candidate-based deduction solver. See ledger.h.
#include "ledger.h"

// Unit u: 0..n-1 rows, n..2n-1 columns, 2n..3n-1 boxes; member i of it.
static int unit_cell(int n, int u, int i)
{
    if (u < n) return cell_at(n, u, i);
    if (u < 2 * n) return cell_at(n, i, u - n);
    int box = u - 2 * n, bw = ledger_box_w(n);
    int r0 = (box / LEDGER_BOX_H) * LEDGER_BOX_H, c0 = (box % LEDGER_BOX_H) * bw;
    return cell_at(n, r0 + i / bw, c0 + i % bw);
}

// --- rules ---------------------------------------------------------------------------

void ledger_board_init(const LedgerPuzzle *p, LedgerBoard *b)
{
    memset(b, 0, sizeof *b);
    memcpy(b->cell, p->given, p->n * p->n);
}

int ledger_conflicts(const LedgerPuzzle *p, const LedgerBoard *b, uint8_t *conflict)
{
    int n = p->n, cells = n * n;
    uint8_t local[PUZZLE_MAX_CELLS];
    uint8_t *f = conflict ? conflict : local;
    memset(f, 0, cells);
    for (int u = 0; u < 3 * n; u++) {
        int seen[PUZZLE_MAX_N + 1] = {0};
        for (int i = 0; i < n; i++) seen[b->cell[unit_cell(n, u, i)]]++;
        for (int i = 0; i < n; i++) {
            int c = unit_cell(n, u, i);
            if (b->cell[c] && seen[b->cell[c]] > 1) f[c] = 1;
        }
    }
    int bad = 0;
    for (int i = 0; i < cells; i++) bad += f[i];
    return bad;
}

bool ledger_solved(const LedgerPuzzle *p, const LedgerBoard *b)
{
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++) if (!b->cell[i]) return false;
    return ledger_conflicts(p, b, NULL) == 0;
}

// --- packing -----------------------------------------------------------------------------

int ledger_payload_len(int n) { return (n * n + 1) / 2 * 2; }

static void put_nibble(uint8_t *out, int idx, int v)
{
    if (idx & 1) out[idx >> 1] |= (uint8_t)(v << 4);
    else         out[idx >> 1] = (uint8_t)(v & 15);
}
static int get_nibble(const uint8_t *in, int idx) { return (idx & 1) ? in[idx >> 1] >> 4 : in[idx >> 1] & 15; }

int ledger_pack(const LedgerPuzzle *p, uint8_t *out)
{
    int n = p->n, cells = n * n, half = (cells + 1) / 2;
    memset(out, 0, 2 * half);
    for (int i = 0; i < cells; i++) {
        put_nibble(out, i, p->given[i]);
        put_nibble(out + half, i, p->solution[i]);
    }
    return 2 * half;
}

bool ledger_unpack(const uint8_t *in, int n, LedgerPuzzle *p)
{
    if (n < LEDGER_MIN_N || n > LEDGER_MAX_N || (n & 1)) return false;
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)n;
    int cells = n * n, half = (cells + 1) / 2;
    for (int i = 0; i < cells; i++) {
        int g = get_nibble(in, i), s = get_nibble(in + half, i);
        if (g > n || s < 1 || s > n || (g && g != s)) return false;
        p->given[i] = (uint8_t)g;
        p->solution[i] = (uint8_t)s;
    }
    return true;
}

// --- deduction -------------------------------------------------------------------------------

typedef struct {
    int n;
    uint16_t cand[PUZZLE_MAX_CELLS];   // bit v set = symbol v still possible (empty cells only)
    LedgerBoard *b;
} LState;

static int bits(uint16_t m) { int k = 0; while (m) { m &= m - 1; k++; } return k; }
static int lowest(uint16_t m) { int v = 0; while (!(m & 1)) { m >>= 1; v++; } return v; }

static void set_cell(LState *s, int c, int v)
{
    int n = s->n;
    s->b->cell[c] = (uint8_t)v;
    s->cand[c] = 0;
    int r = c / n, col = c % n, box = ledger_box_of(n, r, col);
    int units[3] = { r, n + col, 2 * n + box };
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < n; i++) s->cand[unit_cell(n, units[k], i)] &= (uint16_t)~(1 << v);
}

static bool init_state(LState *s, int n, LedgerBoard *b)
{
    s->n = n;
    s->b = b;
    int cells = n * n;
    uint16_t all = (uint16_t)(((1 << (n + 1)) - 1) & ~1);
    for (int i = 0; i < cells; i++) s->cand[i] = b->cell[i] ? 0 : all;
    for (int i = 0; i < cells; i++)
        if (b->cell[i]) {
            int v = b->cell[i];
            b->cell[i] = 0;
            s->cand[i] = (uint16_t)(1 << v);
            set_cell(s, i, v);
        }
    for (int i = 0; i < cells; i++) if (!b->cell[i] && !s->cand[i]) return false;
    return true;
}

static bool consistent(const LState *s)
{
    int n = s->n, cells = n * n;
    for (int i = 0; i < cells; i++) if (!s->b->cell[i] && !s->cand[i]) return false;
    // every unit must still be able to place every symbol
    for (int u = 0; u < 3 * n; u++)
        for (int v = 1; v <= n; v++) {
            bool ok = false;
            for (int i = 0; i < n && !ok; i++) {
                int c = unit_cell(n, u, i);
                ok = s->b->cell[c] == v || (s->cand[c] & (1 << v));
            }
            if (!ok) return false;
        }
    return true;
}

static int tech_naked(LState *s, int *value)
{
    int cells = s->n * s->n;
    for (int c = 0; c < cells; c++)
        if (!s->b->cell[c] && bits(s->cand[c]) == 1) {
            *value = lowest(s->cand[c]);
            set_cell(s, c, *value);
            return c;
        }
    return -1;
}

static int tech_hidden(LState *s, int *value)
{
    int n = s->n;
    for (int u = 0; u < 3 * n; u++)
        for (int v = 1; v <= n; v++) {
            int where = -1, count = 0;
            for (int i = 0; i < n; i++) {
                int c = unit_cell(n, u, i);
                if (s->b->cell[c] == v) { count = -1; break; }
                if (s->cand[c] & (1 << v)) { count++; where = c; }
            }
            if (count == 1) {
                *value = v;
                set_cell(s, where, v);
                return where;
            }
        }
    return -1;
}

// Locked candidates: returns true if some candidate was eliminated.
static bool tech_locked(LState *s)
{
    int n = s->n, bw = ledger_box_w(n);
    for (int box = 0; box < n; box++) {
        int r0 = (box / LEDGER_BOX_H) * LEDGER_BOX_H, c0 = (box % LEDGER_BOX_H) * bw;
        for (int v = 1; v <= n; v++) {
            int rows = 0, cols = 0;
            for (int i = 0; i < n; i++) {
                int c = unit_cell(n, 2 * n + box, i);
                if (s->cand[c] & (1 << v)) { rows |= 1 << (c / n); cols |= 1 << (c % n); }
            }
            if (!rows) continue;
            bool changed = false;
            if (bits(rows) == 1) {                       // pointing: clear the row outside the box
                int r = lowest(rows);
                for (int c = 0; c < n; c++)
                    if ((c < c0 || c >= c0 + bw) && (s->cand[cell_at(n, r, c)] & (1 << v))) {
                        s->cand[cell_at(n, r, c)] &= (uint16_t)~(1 << v);
                        changed = true;
                    }
            }
            if (bits(cols) == 1) {
                int c = lowest(cols);
                for (int r = 0; r < n; r++)
                    if ((r < r0 || r >= r0 + LEDGER_BOX_H) && (s->cand[cell_at(n, r, c)] & (1 << v))) {
                        s->cand[cell_at(n, r, c)] &= (uint16_t)~(1 << v);
                        changed = true;
                    }
            }
            if (changed) return true;
        }
    }
    // claiming: a symbol confined to one box within a row/column
    for (int line = 0; line < 2 * n; line++)
        for (int v = 1; v <= n; v++) {
            int boxes = 0;
            for (int i = 0; i < n; i++) {
                int c = unit_cell(n, line, i);
                if (s->cand[c] & (1 << v)) boxes |= 1 << ledger_box_of(n, c / n, c % n);
            }
            if (!boxes || bits(boxes) != 1) continue;
            int box = lowest(boxes);
            bool changed = false;
            for (int i = 0; i < n; i++) {
                int c = unit_cell(n, 2 * n + box, i);
                bool on_line = line < n ? c / n == line : c % n == line - n;
                if (!on_line && (s->cand[c] & (1 << v))) {
                    s->cand[c] &= (uint16_t)~(1 << v);
                    changed = true;
                }
            }
            if (changed) return true;
        }
    return false;
}

// One step: a placement (returns technique, sets cell/value) or, after
// locked-candidate eliminations, the placement they enable. -1 when stuck.
static int deduce_step(LState *s, int *cell, int *value)
{
    if (!consistent(s)) return -2;
    int c;
    // scanning for the only place of a symbol comes naturally before
    // working out the only symbol a cell can take
    if ((c = tech_hidden(s, value)) >= 0) { *cell = c; return LEDT_HIDDEN; }
    if ((c = tech_naked(s, value)) >= 0) { *cell = c; return LEDT_NAKED; }
    int guard = 0;
    while (tech_locked(s) && guard++ < 64) {
        if ((c = tech_hidden(s, value)) >= 0) { *cell = c; return LEDT_LOCKED; }
        if ((c = tech_naked(s, value)) >= 0) { *cell = c; return LEDT_LOCKED; }
    }
    return -1;
}

void ledger_deduce(const LedgerPuzzle *p, const LedgerBoard *start, LedgerSolveStats *st, LedgerBoard *out)
{
    LedgerBoard b;
    if (start) b = *start; else ledger_board_init(p, &b);
    memset(st, 0, sizeof *st);
    st->hardest = -1;
    LState s;
    int n = p->n, cells = n * n;
    if (init_state(&s, n, &b)) {
        for (;;) {
            int filled = 0;
            for (int i = 0; i < cells; i++) filled += b.cell[i] != 0;
            if (filled == cells) break;
            int cell, value;
            int t = deduce_step(&s, &cell, &value);
            if (t < 0) break;
            st->steps[t]++;
            if (t > st->hardest) st->hardest = t;
        }
    }
    st->solved = ledger_solved(p, &b);
    if (out) *out = b;
}

bool ledger_next_step(const LedgerPuzzle *p, const LedgerBoard *b, int *cell, int *value)
{
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++)
        if (b->cell[i] && b->cell[i] != p->solution[i]) return false;
    LedgerBoard t = *b;
    LState s;
    if (!init_state(&s, p->n, &t)) return false;
    return deduce_step(&s, cell, value) >= 0;
}

// --- counting ---------------------------------------------------------------------------------

typedef struct { const LedgerPuzzle *p; LedgerBoard b; int cap, found; } LCount;

static bool can_place(const LedgerBoard *b, int n, int c, int v)
{
    int r = c / n, col = c % n, box = ledger_box_of(n, r, col);
    int units[3] = { r, n + col, 2 * n + box };
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < n; i++) if (b->cell[unit_cell(n, units[k], i)] == v) return false;
    return true;
}

static void count_rec(LCount *k, int i)
{
    if (k->found >= k->cap) return;
    int n = k->p->n, cells = n * n;
    while (i < cells && k->b.cell[i]) i++;
    if (i == cells) { k->found++; return; }
    for (int v = 1; v <= n; v++)
        if (can_place(&k->b, n, i, v)) {
            k->b.cell[i] = (uint8_t)v;
            count_rec(k, i + 1);
            k->b.cell[i] = 0;
        }
}

int ledger_count_solutions(const LedgerPuzzle *p, int cap)
{
    LCount k;
    k.p = p;
    ledger_board_init(p, &k.b);
    k.cap = cap;
    k.found = 0;
    count_rec(&k, 0);
    return k.found;
}

// --- difficulty --------------------------------------------------------------------------------

int ledger_difficulty(const LedgerPuzzle *p, const LedgerSolveStats *st)
{
    if (!st->solved) return 0;
    int n = p->n, cells = n * n, givens = 0;
    for (int i = 0; i < cells; i++) givens += p->given[i] != 0;
    int empty_pct = (cells - givens) * 100 / cells;
    int d = empty_pct < 40 ? 1 : empty_pct < 50 ? 2 : empty_pct < 58 ? 3 : empty_pct < 66 ? 4 : 5;
    int naked = st->steps[LEDT_NAKED], locked = st->steps[LEDT_LOCKED];
    d += naked == 0 ? 0 : naked <= 2 ? 1 : 2;             // candidate bookkeeping needed
    d += locked == 0 ? 0 : locked == 1 ? 2 : 3;
    d += n >= 8 ? 2 : n >= 6 ? 1 : 0;
    return clamp_difficulty(d);
}
