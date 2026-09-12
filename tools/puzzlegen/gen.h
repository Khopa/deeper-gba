// puzzlegen — PC-side puzzle generation. One generator per family, a shared
// RNG, a canonicaliser (dedup under the 8 symmetries of the square) and a
// bank writer. The rules/solvers themselves live in common/ and are the very
// same code the ROM runs.
#ifndef GEN_H
#define GEN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "puzzle_common.h"

// --- deterministic RNG (splitmix64) -------------------------------------------
typedef struct { uint64_t s; } Rng;
void     rng_init(Rng *r, uint64_t seed);
uint64_t rng_u64(Rng *r);
int      rng_below(Rng *r, int n);            // uniform in [0, n)
void     rng_shuffle(Rng *r, int *a, int n);

// --- canonical form under the dihedral group ------------------------------------
// The 8 transforms of an n x n grid: index t = rotation (t & 3) then flip (t & 4).
void     canon_transform_cell(int n, int t, int r, int c, int *out_r, int *out_c);
// Canonical form of a label grid (values are relabelled by first appearance);
// `out` receives the canonical grid, returns the transform index that produced it.
int      canon_labels(int n, const uint8_t *in, uint8_t *out);
// Canonical form of a plain value grid (values kept as-is).
int      canon_values(int n, const uint8_t *in, uint8_t *out);
uint64_t fnv1a64(const void *data, int len);

// --- dedup set ------------------------------------------------------------------
typedef struct {
    uint64_t *keys;
    int count, cap;
} HashSet;
void hs_init(HashSet *h);
bool hs_insert(HashSet *h, uint64_t key);    // false if already present
void hs_free(HashSet *h);

// --- bank -----------------------------------------------------------------------
// A generated puzzle ready to be written: header + packed payload.
typedef struct {
    PuzzleHeader hdr;
    uint8_t      payload[128];
    int          payload_len;
    uint64_t     canon_key;                   // dedup key
} GenRecord;

typedef struct {
    GenRecord *rec;
    int count, cap;
} GenBank;
void gbank_init(GenBank *b);
void gbank_add(GenBank *b, const GenRecord *r);
void gbank_sort_by_difficulty(GenBank *b);
bool gbank_write(const GenBank *b, const char *path);   // see docs/puzzle_bank.md
void gbank_free(GenBank *b);

// --- family generators ------------------------------------------------------------
// Each attempt builds one candidate; returns true when it is valid, unique,
// solvable by deduction alone and not a duplicate (the set is updated).
typedef bool (*GenAttempt)(Rng *rng, int size, HashSet *seen, GenRecord *out);

typedef struct {
    const char *name;
    int         family;
    int         min_size, max_size;
    GenAttempt  attempt;
} FamilyGen;

extern const FamilyGen gen_dig;
extern const FamilyGen gen_vein;

// text dump for debugging / docs
void dump_record(FILE *f, const GenRecord *r);

#endif
