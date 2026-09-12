// DIG — the signature placement puzzle.
//
// An n x n grid is split into n rock regions. The player must place exactly
// one dig per row, per column and per region, and no two digs may touch,
// not even diagonally. Cells can also be marked with a cross ("no dig here")
// as a note; crosses never count as errors.
//
// Record payload (after PuzzleHeader, size = n):
//     region[n*n]  packed 4 bits per cell (low nibble first)
//     solution[n]  packed 4 bits per row: column of the dig
#ifndef DIG_H
#define DIG_H

#include "puzzle_common.h"

#define DIG_MIN_N 5
#define DIG_MAX_N 8

typedef struct {
    uint8_t n;
    uint8_t region[PUZZLE_MAX_CELLS];   // region id 0..n-1 per cell
    uint8_t solution[PUZZLE_MAX_N];     // column of the dig in each row
} DigPuzzle;

enum DigMark { DIG_EMPTY = 0, DIG_CROSS = 1, DIG_DIG = 2 };

typedef struct {
    uint8_t cell[PUZZLE_MAX_CELLS];     // enum DigMark
} DigBoard;

// --- rules -------------------------------------------------------------------
// Fill `conflict` (one byte per cell, may be NULL) with 1 for every dig that
// breaks a rule; return the number of conflicting digs.
int  dig_conflicts(const DigPuzzle *p, const DigBoard *b, uint8_t *conflict);
// True when every row holds exactly one dig and nothing conflicts.
bool dig_solved(const DigPuzzle *p, const DigBoard *b);
int  dig_count_digs(const DigBoard *b);

// --- packing -----------------------------------------------------------------
int  dig_payload_len(int n);                         // bytes after the header
int  dig_pack(const DigPuzzle *p, uint8_t *out);      // returns bytes written
bool dig_unpack(const uint8_t *in, int n, DigPuzzle *p);

// --- deduction solver (human-style, no guessing) ------------------------------
// Techniques, from the most obvious to the hardest. The generator rejects
// puzzles that this solver cannot finish, so every bank puzzle is solvable
// without trial and error, and the difficulty score is derived from the
// techniques used.
enum DigTechnique {
    DIGT_SINGLE = 0,     // a row/column/region has one candidate left
    DIGT_CONFINE,        // a region's candidates all lie in one row/column (or a row's in one region)
    DIGT_PAIRS,          // k regions confined to the same k rows/columns
    DIGT_TRIAL,          // placing here would empty some row/column/region
    DIGT_COUNT
};

typedef struct {
    int      steps[DIGT_COUNT];   // how many times each technique fired
    int      hardest;             // highest technique index used (-1 if none)
    bool     solved;
    uint64_t candidates;          // remaining candidate cells when stopping
    uint64_t placed;              // digs placed
} DigSolveStats;

// Run the deduction solver from `start` (may be NULL = empty board).
// `cross` cells of the start board are removed from the candidates.
void dig_deduce(const DigPuzzle *p, const DigBoard *start, DigSolveStats *st);

// One deduction step from a board: either a cell that can be safely crossed
// (kind = DIG_CROSS) or a forced dig (kind = DIG_DIG). Used for hints.
// Returns false if no single step is found (or the board is contradictory).
bool dig_next_step(const DigPuzzle *p, const DigBoard *b, int *cell, int *kind);

// Backtracking count of solutions, capped at `cap` (2 = uniqueness test).
int  dig_count_solutions(const DigPuzzle *p, int cap);

// Map solver stats + size to the shared 1..10 scale.
int  dig_difficulty(const DigPuzzle *p, const DigSolveStats *st);

#endif
