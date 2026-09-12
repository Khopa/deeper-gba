// VEIN generator: fill a random valid grid, then take givens away as long as
// the deduction solver (common/vein.c) still finishes the puzzle. Removing
// only a random share of the cells spreads the results over the difficulty
// scale (more givens = easier).
#include <string.h>
#include "gen.h"
#include "vein.h"

static bool fill_rec(Rng *rng, VeinPuzzle *p, VeinBoard *b, int i)
{
    int n = p->n, cells = n * n;
    if (i == cells) return true;
    int first = rng_below(rng, 2);
    for (int k = 0; k < 2; k++) {
        int v = ((first + k) & 1) ? VEIN_DARK : VEIN_LIGHT;
        b->cell[i] = (uint8_t)v;
        if (vein_conflicts(p, b, NULL) == 0 && fill_rec(rng, p, b, i + 1)) return true;
    }
    b->cell[i] = 0;
    return false;
}

static uint64_t vein_key(const VeinPuzzle *p)
{
    int n = p->n;
    uint8_t g[PUZZLE_MAX_CELLS], swapped[PUZZLE_MAX_CELLS], a[PUZZLE_MAX_CELLS], b[PUZZLE_MAX_CELLS];
    memcpy(g, p->given, n * n);
    for (int i = 0; i < n * n; i++) swapped[i] = g[i] ? (uint8_t)(3 - g[i]) : 0;
    canon_values(n, g, a);
    canon_values(n, swapped, b);
    const uint8_t *best = memcmp(a, b, n * n) <= 0 ? a : b;
    return fnv1a64(best, n * n) ^ ((uint64_t)n << 56) ^ ((uint64_t)FAM_VEIN << 48);
}

static bool vein_attempt(Rng *rng, int n, HashSet *seen, GenRecord *out)
{
    VeinPuzzle p;
    VeinBoard b;
    memset(&p, 0, sizeof p);
    memset(&b, 0, sizeof b);
    p.n = (uint8_t)n;
    if (!fill_rec(rng, &p, &b, 0)) return false;
    memcpy(p.solution, b.cell, n * n);
    memcpy(p.given, b.cell, n * n);

    int cells = n * n;
    int order[PUZZLE_MAX_CELLS];
    for (int i = 0; i < cells; i++) order[i] = i;
    rng_shuffle(rng, order, cells);
    int share = 35 + rng_below(rng, 66);           // percent of cells we try to clear
    VeinSolveStats st;
    for (int k = 0; k < cells * share / 100; k++) {
        int i = order[k];
        uint8_t saved = p.given[i];
        p.given[i] = VEIN_EMPTY;
        vein_deduce(&p, NULL, &st, NULL);
        if (!st.solved) p.given[i] = saved;
    }
    vein_deduce(&p, NULL, &st, NULL);
    if (!st.solved) return false;
    int givens = 0;
    for (int i = 0; i < cells; i++) givens += p.given[i] != 0;
    if (givens > cells * 2 / 3) return false;      // too full to be a puzzle
    if (vein_count_solutions(&p, 2) != 1) return false;

    uint64_t key = vein_key(&p);
    if (!hs_insert(seen, key)) return false;

    memset(out, 0, sizeof *out);
    out->hdr.family = FAM_VEIN;
    out->hdr.size = (uint8_t)n;
    out->hdr.difficulty = (uint8_t)vein_difficulty(&p, &st);
    out->hdr.flags = (uint8_t)st.hardest;
    out->payload_len = vein_pack(&p, out->payload);
    out->canon_key = key;
    return true;
}

const FamilyGen gen_vein = { "vein", FAM_VEIN, VEIN_MIN_N, VEIN_MAX_N, vein_attempt };
