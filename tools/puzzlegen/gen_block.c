// BLOCK generator: carve a cavity, cut it into random blocks of 3 to 5
// cells, keep the set only if it tiles the cavity in exactly one way.
#include <stdlib.h>
#include <string.h>
#include "gen.h"
#include "block.h"

static const int dr[4] = { -1, 0, 1, 0 }, dc[4] = { 0, 1, 0, -1 };

static bool cut_pieces(Rng *rng, BlockPuzzle *p)
{
    int n = p->n, cells = n * n;
    int owner[PUZZLE_MAX_CELLS];
    for (int i = 0; i < cells; i++) owner[i] = p->cavity[i] ? -1 : -2;
    p->count = 0;
    for (;;) {
        int start = -1;
        for (int i = 0; i < cells && start < 0; i++) if (owner[i] == -1) start = i;
        if (start < 0) break;
        if (p->count == BLOCK_MAX_PIECES) return false;
        int roll = rng_below(rng, 100);            // bigger blocks leave fewer alternative tilings
        int target = roll < 15 ? 3 : roll < 50 ? 4 : 5;
        int mine[BLOCK_MAX_SIZE], size = 0;
        mine[size++] = start;
        owner[start] = p->count;
        while (size < target) {
            int cand[BLOCK_MAX_SIZE * 4], nc = 0;
            for (int i = 0; i < size; i++)
                for (int d = 0; d < 4; d++) {
                    int r = mine[i] / n + dr[d], c = mine[i] % n + dc[d];
                    if (r < 0 || r >= n || c < 0 || c >= n) continue;
                    int cell = cell_at(n, r, c);
                    if (owner[cell] == -1) cand[nc++] = cell;
                }
            if (!nc) break;
            int pick = cand[rng_below(rng, nc)];
            owner[pick] = p->count;
            mine[size++] = pick;
        }
        if (size < 3) return false;                 // a leftover too small to be a block
        BlockShape s;
        memset(&s, 0, sizeof s);
        s.size = (uint8_t)size;
        for (int i = 0; i < size; i++) { s.r[i] = (uint8_t)(mine[i] / n); s.c[i] = (uint8_t)(mine[i] % n); }
        int shape, orient;
        if (!block_shape_identify(&s, &shape, &orient)) return false;
        int minr = 99, minc = 99;
        for (int i = 0; i < size; i++) { if (s.r[i] < minr) minr = s.r[i]; if (s.c[i] < minc) minc = s.c[i]; }
        p->shape[p->count] = (uint8_t)shape;
        p->sol_orient[p->count] = (uint8_t)orient;
        p->sol_anchor[p->count] = (uint8_t)cell_at(n, minr, minc);
        p->count++;
    }
    return p->count >= 3;
}

static int cmp_u8(const void *a, const void *b) { return *(const uint8_t *)a - *(const uint8_t *)b; }

static uint64_t block_key(const BlockPuzzle *p)
{
    int n = p->n;
    uint8_t canon[PUZZLE_MAX_CELLS], shapes[BLOCK_MAX_PIECES];
    canon_values(n, p->cavity, canon);
    memcpy(shapes, p->shape, p->count);
    qsort(shapes, p->count, 1, cmp_u8);
    uint64_t h = fnv1a64(canon, n * n);
    h ^= fnv1a64(shapes, p->count) * 0x9E3779B97F4A7C15ull;
    return h ^ ((uint64_t)n << 56) ^ ((uint64_t)FAM_BLOCK << 48);
}

static bool block_attempt(Rng *rng, int n, HashSet *seen, GenRecord *out)
{
    BlockPuzzle p;
    memset(&p, 0, sizeof p);
    p.n = (uint8_t)n;
    int cells = n * n;
    for (int i = 0; i < cells; i++) p.cavity[i] = 1;
    int rocks = 2 + rng_below(rng, n * n / 5);   // an irregular cavity constrains the blocks
    for (int k = 0; k < rocks; k++) p.cavity[rng_below(rng, cells)] = 0;
    p.open_cells = 0;
    for (int i = 0; i < cells; i++) p.open_cells += p.cavity[i];
    if (p.open_cells < 12) return false;

    if (!cut_pieces(rng, &p)) return false;
    long effort = 0;
    if (block_count_solutions(&p, 2, &effort) != 1) return false;

    uint64_t key = block_key(&p);
    if (!hs_insert(seen, key)) return false;

    memset(out, 0, sizeof *out);
    out->hdr.family = FAM_BLOCK;
    out->hdr.size = (uint8_t)n;
    out->hdr.difficulty = (uint8_t)block_difficulty(&p, effort);
    out->hdr.flags = p.count;
    out->payload_len = block_pack(&p, out->payload);
    out->canon_key = key;
    return true;
}

const FamilyGen gen_block = { "block", FAM_BLOCK, BLOCK_MIN_N, BLOCK_MAX_N, block_attempt };
