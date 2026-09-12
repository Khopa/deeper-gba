// DIG generator: draw a random valid dig layout, grow one rock region around
// each dig, then keep the grid only if the solution is unique and the
// deduction solver (common/dig.c) finishes without guessing.
#include <string.h>
#include "gen.h"
#include "dig.h"

// Random non-adjacent permutation (one dig per row/column, consecutive rows
// at least two columns apart), by randomised backtracking.
static bool random_solution_rec(Rng *rng, int n, int r, uint8_t *sol, uint8_t *used)
{
    if (r == n) return true;
    int order[PUZZLE_MAX_N];
    for (int c = 0; c < n; c++) order[c] = c;
    rng_shuffle(rng, order, n);
    for (int k = 0; k < n; k++) {
        int c = order[k];
        if (used[c]) continue;
        if (r > 0 && (c == sol[r - 1] - 1 || c == sol[r - 1] + 1)) continue;
        used[c] = 1;
        sol[r] = (uint8_t)c;
        if (random_solution_rec(rng, n, r + 1, sol, used)) return true;
        used[c] = 0;
    }
    return false;
}

// Grow regions from the dig cells by random frontier expansion. Regions
// whose growth weight is low stay small, which produces the narrow "veins"
// that make deductions interesting.
static void grow_regions(Rng *rng, int n, const uint8_t *sol, uint8_t *region)
{
    int cells = n * n;
    memset(region, 0xFF, cells);
    int weight[PUZZLE_MAX_N];
    for (int r = 0; r < n; r++) {
        region[cell_at(n, r, sol[r])] = (uint8_t)r;
        weight[r] = 1 + rng_below(rng, 4);
    }
    static const int dr[4] = { -1, 1, 0, 0 }, dc[4] = { 0, 0, -1, 1 };
    for (;;) {
        // candidate (cell, region) pairs on the frontier, weighted
        int cand_cell[PUZZLE_MAX_CELLS * 4], cand_reg[PUZZLE_MAX_CELLS * 4], total = 0;
        for (int i = 0; i < cells; i++) {
            if (region[i] != 0xFF) continue;
            int r = i / n, c = i % n;
            for (int d = 0; d < 4; d++) {
                int rr = r + dr[d], cc = c + dc[d];
                if (rr < 0 || rr >= n || cc < 0 || cc >= n) continue;
                int reg = region[cell_at(n, rr, cc)];
                if (reg == 0xFF) continue;
                for (int w = 0; w < weight[reg]; w++) {
                    cand_cell[total] = i;
                    cand_reg[total] = reg;
                    total++;
                }
            }
        }
        if (!total) break;
        int k = rng_below(rng, total);
        region[cand_cell[k]] = (uint8_t)cand_reg[k];
    }
}

static bool dig_attempt(Rng *rng, int n, HashSet *seen, GenRecord *out)
{
    DigPuzzle p;
    memset(&p, 0, sizeof p);
    p.n = (uint8_t)n;
    uint8_t used[PUZZLE_MAX_N] = {0};
    if (!random_solution_rec(rng, n, 0, p.solution, used)) return false;
    grow_regions(rng, n, p.solution, p.region);

    if (dig_count_solutions(&p, 2) != 1) return false;
    DigSolveStats st;
    dig_deduce(&p, NULL, &st);
    if (!st.solved) return false;

    uint8_t canon[PUZZLE_MAX_CELLS];
    canon_labels(n, p.region, canon);
    uint64_t key = fnv1a64(canon, n * n) ^ ((uint64_t)n << 56);
    if (!hs_insert(seen, key)) return false;

    memset(out, 0, sizeof *out);
    out->hdr.family = FAM_DIG;
    out->hdr.size = (uint8_t)n;
    out->hdr.difficulty = (uint8_t)dig_difficulty(&p, &st);
    out->hdr.flags = (uint8_t)st.hardest;
    out->payload_len = dig_pack(&p, out->payload);
    out->canon_key = key;
    return true;
}

const FamilyGen gen_dig = { "dig", FAM_DIG, DIG_MIN_N, DIG_MAX_N, dig_attempt };
