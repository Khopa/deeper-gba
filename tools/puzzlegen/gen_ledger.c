// LEDGER generator: random complete grid by backtracking, then givens are
// carved away while the deduction solver (common/ledger.c) still finishes.
#include <string.h>
#include "gen.h"
#include "ledger.h"

static bool fill_rec(Rng *rng, LedgerPuzzle *p, LedgerBoard *b, int i)
{
    int n = p->n, cells = n * n;
    if (i == cells) return true;
    int order[PUZZLE_MAX_N];
    for (int v = 0; v < n; v++) order[v] = v + 1;
    rng_shuffle(rng, order, n);
    for (int k = 0; k < n; k++) {
        b->cell[i] = (uint8_t)order[k];
        if (ledger_conflicts(p, b, NULL) == 0 && fill_rec(rng, p, b, i + 1)) return true;
    }
    b->cell[i] = 0;
    return false;
}

// Symmetries that keep 2 x n/2 boxes: all eight for 4x4 (square boxes),
// only identity, 180° rotation and the two mirrors for 6x6. Symbols are
// relabelled by first appearance (0 stays 0).
static uint64_t ledger_key(const LedgerPuzzle *p)
{
    int n = p->n;
    int mask = n == 4 ? 0xFF : (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);
    uint8_t best[PUZZLE_MAX_CELLS], cur[PUZZLE_MAX_CELLS];
    bool have = false;
    for (int t = 0; t < 8; t++) {
        if (!(mask & (1 << t))) continue;
        for (int r = 0; r < n; r++)
            for (int c = 0; c < n; c++) {
                int tr, tc;
                canon_transform_cell(n, t, r, c, &tr, &tc);
                cur[tr * n + tc] = p->given[r * n + c];
            }
        int map[PUZZLE_MAX_N + 1], next = 1;
        for (int v = 0; v <= n; v++) map[v] = 0;
        for (int i = 0; i < n * n; i++)
            if (cur[i]) {
                if (!map[cur[i]]) map[cur[i]] = next++;
                cur[i] = (uint8_t)map[cur[i]];
            }
        if (!have || memcmp(cur, best, n * n) < 0) { memcpy(best, cur, n * n); have = true; }
    }
    return fnv1a64(best, n * n) ^ ((uint64_t)n << 56) ^ ((uint64_t)FAM_LEDGER << 48);
}

static bool ledger_attempt(Rng *rng, int n, HashSet *seen, GenRecord *out)
{
    LedgerPuzzle p;
    LedgerBoard b;
    memset(&p, 0, sizeof p);
    memset(&b, 0, sizeof b);
    p.n = (uint8_t)n;
    if (!fill_rec(rng, &p, &b, 0)) return false;
    int cells = n * n;
    memcpy(p.solution, b.cell, cells);
    memcpy(p.given, b.cell, cells);

    int order[PUZZLE_MAX_CELLS];
    for (int i = 0; i < cells; i++) order[i] = i;
    rng_shuffle(rng, order, cells);
    int share = 40 + rng_below(rng, 61);
    LedgerSolveStats st;
    for (int k = 0; k < cells * share / 100; k++) {
        int i = order[k];
        uint8_t saved = p.given[i];
        p.given[i] = 0;
        ledger_deduce(&p, NULL, &st, NULL);
        if (!st.solved) p.given[i] = saved;
    }
    ledger_deduce(&p, NULL, &st, NULL);
    if (!st.solved) return false;
    int givens = 0;
    for (int i = 0; i < cells; i++) givens += p.given[i] != 0;
    if (givens > cells * 3 / 4) return false;
    if (ledger_count_solutions(&p, 2) != 1) return false;

    uint64_t key = ledger_key(&p);
    if (!hs_insert(seen, key)) return false;

    memset(out, 0, sizeof *out);
    out->hdr.family = FAM_LEDGER;
    out->hdr.size = (uint8_t)n;
    out->hdr.difficulty = (uint8_t)ledger_difficulty(&p, &st);
    out->hdr.flags = (uint8_t)st.hardest;
    out->payload_len = ledger_pack(&p, out->payload);
    out->canon_key = key;
    return true;
}

const FamilyGen gen_ledger = { "ledger", FAM_LEDGER, LEDGER_MIN_N, LEDGER_MAX_N, ledger_attempt };
