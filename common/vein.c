// VEIN rules, packing and deduction solver. See vein.h.
#include "vein.h"

static int other(int v) { return v == VEIN_LIGHT ? VEIN_DARK : VEIN_LIGHT; }

// Line access: line k < n is row k, line k >= n is column k - n.
static int line_cell(int n, int line, int i)
{
    return line < n ? cell_at(n, line, i) : cell_at(n, i, line - n);
}

// --- rules -----------------------------------------------------------------------

void vein_board_init(const VeinPuzzle *p, VeinBoard *b)
{
    memset(b, 0, sizeof *b);
    memcpy(b->cell, p->given, p->n * p->n);
}

int vein_conflicts(const VeinPuzzle *p, const VeinBoard *b, uint8_t *conflict)
{
    int n = p->n, cells = n * n, half = n / 2;
    uint8_t local[PUZZLE_MAX_CELLS];
    uint8_t *f = conflict ? conflict : local;
    memset(f, 0, cells);
    for (int line = 0; line < 2 * n; line++) {
        int cnt[3] = { 0, 0, 0 };
        for (int i = 0; i < n; i++) {
            int v = b->cell[line_cell(n, line, i)];
            cnt[v]++;
            if (i >= 2 && v && b->cell[line_cell(n, line, i - 1)] == v && b->cell[line_cell(n, line, i - 2)] == v)
                f[line_cell(n, line, i)] = f[line_cell(n, line, i - 1)] = f[line_cell(n, line, i - 2)] = 1;
        }
        for (int v = VEIN_LIGHT; v <= VEIN_DARK; v++)
            if (cnt[v] > half)
                for (int i = 0; i < n; i++)
                    if (b->cell[line_cell(n, line, i)] == v) f[line_cell(n, line, i)] = 1;
    }
    int bad = 0;
    for (int i = 0; i < cells; i++) bad += f[i];
    return bad;
}

bool vein_solved(const VeinPuzzle *p, const VeinBoard *b)
{
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++) if (!b->cell[i]) return false;
    return vein_conflicts(p, b, NULL) == 0;
}

// --- packing -----------------------------------------------------------------------

int vein_payload_len(int n) { return (n * n + 3) / 4 + (n * n + 7) / 8; }

int vein_pack(const VeinPuzzle *p, uint8_t *out)
{
    int n = p->n, cells = n * n, len = vein_payload_len(n);
    memset(out, 0, len);
    for (int i = 0; i < cells; i++) out[i >> 2] |= (uint8_t)((p->given[i] & 3) << ((i & 3) * 2));
    uint8_t *sol = out + (cells + 3) / 4;
    for (int i = 0; i < cells; i++) if (p->solution[i] == VEIN_DARK) sol[i >> 3] |= (uint8_t)(1 << (i & 7));
    return len;
}

bool vein_unpack(const uint8_t *in, int n, VeinPuzzle *p)
{
    if (n < VEIN_MIN_N || n > VEIN_MAX_N || (n & 1)) return false;
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)n;
    int cells = n * n;
    const uint8_t *sol = in + (cells + 3) / 4;
    for (int i = 0; i < cells; i++) {
        int g = (in[i >> 2] >> ((i & 3) * 2)) & 3;
        if (g > VEIN_DARK) return false;
        p->given[i] = (uint8_t)g;
        p->solution[i] = (sol[i >> 3] >> (i & 7)) & 1 ? VEIN_DARK : VEIN_LIGHT;
        if (g && g != p->solution[i]) return false;
    }
    return true;
}

// --- deduction --------------------------------------------------------------------------

// A line is consistent when it has no triple and no ore over n/2.
static bool line_ok(const VeinBoard *b, int n, int line)
{
    int cnt[3] = { 0, 0, 0 };
    for (int i = 0; i < n; i++) {
        int v = b->cell[line_cell(n, line, i)];
        cnt[v]++;
        if (i >= 2 && v && b->cell[line_cell(n, line, i - 1)] == v && b->cell[line_cell(n, line, i - 2)] == v)
            return false;
    }
    return cnt[VEIN_LIGHT] <= n / 2 && cnt[VEIN_DARK] <= n / 2;
}

// Technique 0 on one line: returns the filled cell index or -1.
static int line_triple(VeinBoard *b, int n, int line, int *value)
{
    for (int i = 0; i < n; i++) {
        int c = line_cell(n, line, i);
        if (b->cell[c]) continue;
        int a = i >= 1 ? b->cell[line_cell(n, line, i - 1)] : 0;
        int aa = i >= 2 ? b->cell[line_cell(n, line, i - 2)] : 0;
        int d = i + 1 < n ? b->cell[line_cell(n, line, i + 1)] : 0;
        int dd = i + 2 < n ? b->cell[line_cell(n, line, i + 2)] : 0;
        int forced = 0;
        if (a && a == aa) forced = other(a);
        else if (d && d == dd) forced = other(d);
        else if (a && a == d) forced = other(a);
        if (forced) {
            b->cell[c] = (uint8_t)forced;
            *value = forced;
            return c;
        }
    }
    return -1;
}

// Technique 1 on one line.
static int line_count(VeinBoard *b, int n, int line, int *value)
{
    int cnt[3] = { 0, 0, 0 };
    for (int i = 0; i < n; i++) cnt[b->cell[line_cell(n, line, i)]]++;
    for (int v = VEIN_LIGHT; v <= VEIN_DARK; v++) {
        if (cnt[v] != n / 2 || !cnt[0]) continue;
        for (int i = 0; i < n; i++) {
            int c = line_cell(n, line, i);
            if (!b->cell[c]) {
                b->cell[c] = (uint8_t)other(v);
                *value = other(v);
                return c;
            }
        }
    }
    return -1;
}

// Propagate techniques 0 and 1 on the row and column of `cell` until nothing
// changes; false on contradiction.
static bool propagate_local(VeinBoard *b, int n, int cell)
{
    int lines[2] = { cell / n, n + cell % n };
    bool changed = true;
    while (changed) {
        changed = false;
        for (int k = 0; k < 2; k++) {
            int v;
            if (!line_ok(b, n, lines[k])) return false;
            if (line_triple(b, n, lines[k], &v) >= 0 || line_count(b, n, lines[k], &v) >= 0) changed = true;
        }
    }
    for (int k = 0; k < 2; k++) if (!line_ok(b, n, lines[k])) return false;
    return true;
}

// Technique 2: trial on every empty cell.
static int trial(VeinBoard *b, int n, int *value)
{
    int cells = n * n;
    for (int c = 0; c < cells; c++) {
        if (b->cell[c]) continue;
        for (int v = VEIN_LIGHT; v <= VEIN_DARK; v++) {
            VeinBoard t = *b;
            t.cell[c] = (uint8_t)v;
            if (!propagate_local(&t, n, c)) {
                b->cell[c] = (uint8_t)other(v);
                *value = other(v);
                return c;
            }
        }
    }
    return -1;
}

static int deduce_step(VeinBoard *b, int n, int *cell, int *value)
{
    for (int line = 0; line < 2 * n; line++) {
        if (!line_ok(b, n, line)) return -2;         // contradiction
        int c = line_triple(b, n, line, value);
        if (c >= 0) { *cell = c; return VEINT_TRIPLE; }
    }
    for (int line = 0; line < 2 * n; line++) {
        int c = line_count(b, n, line, value);
        if (c >= 0) { *cell = c; return VEINT_COUNT; }
    }
    int c = trial(b, n, value);
    if (c >= 0) { *cell = c; return VEINT_TRIAL; }
    return -1;
}

void vein_deduce(const VeinPuzzle *p, const VeinBoard *start, VeinSolveStats *st, VeinBoard *out)
{
    VeinBoard b;
    if (start) b = *start; else vein_board_init(p, &b);
    memset(st, 0, sizeof *st);
    st->hardest = -1;
    int n = p->n, cells = n * n;
    for (;;) {
        int filled = 0;
        for (int i = 0; i < cells; i++) filled += b.cell[i] != 0;
        if (filled == cells) break;
        int cell, value;
        int t = deduce_step(&b, n, &cell, &value);
        if (t < 0) break;
        st->steps[t]++;
        if (t > st->hardest) st->hardest = t;
    }
    st->solved = vein_solved(p, &b);
    if (out) *out = b;
}

bool vein_next_step(const VeinPuzzle *p, const VeinBoard *b, int *cell, int *value)
{
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++)
        if (b->cell[i] && b->cell[i] != p->solution[i]) return false;
    VeinBoard t = *b;
    int t_idx = deduce_step(&t, p->n, cell, value);
    return t_idx >= 0;
}

// --- counting ----------------------------------------------------------------------------

typedef struct { const VeinPuzzle *p; VeinBoard b; int cap, found; } VeinCount;

static void count_rec(VeinCount *k, int i)
{
    if (k->found >= k->cap) return;
    int n = k->p->n, cells = n * n;
    while (i < cells && k->b.cell[i]) i++;
    if (i == cells) { k->found++; return; }
    for (int v = VEIN_LIGHT; v <= VEIN_DARK; v++) {
        k->b.cell[i] = (uint8_t)v;
        if (line_ok(&k->b, n, i / n) && line_ok(&k->b, n, n + i % n)) count_rec(k, i + 1);
        k->b.cell[i] = 0;
    }
}

int vein_count_solutions(const VeinPuzzle *p, int cap)
{
    VeinCount k;
    k.p = p;
    vein_board_init(p, &k.b);
    k.cap = cap;
    k.found = 0;
    count_rec(&k, 0);
    return k.found;
}

// --- difficulty ----------------------------------------------------------------------------

int vein_difficulty(const VeinPuzzle *p, const VeinSolveStats *st)
{
    if (!st->solved) return 0;
    int n = p->n, cells = n * n, givens = 0;
    for (int i = 0; i < cells; i++) givens += p->given[i] != 0;
    int empty_pct = (cells - givens) * 100 / cells;
    // sparser givens mean longer chains; counting arguments and trials add on top
    int d = empty_pct < 45 ? 1 : empty_pct < 55 ? 2 : empty_pct < 62 ? 3 : empty_pct < 68 ? 4 : 5;
    if (st->steps[VEINT_COUNT] >= 12) d += 1;
    int trials = st->steps[VEINT_TRIAL];
    d += trials == 0 ? 0 : trials == 1 ? 2 : trials == 2 ? 3 : 4;
    if (n >= 8) d += 1;
    return clamp_difficulty(d);
}
