// VEIN — balancing two ores.
//
// An n x n grid (n even) must be filled with two ore types so that every row
// and every column holds exactly n/2 of each, and no three consecutive cells
// in a row or column carry the same ore. Some cells are given.
//
// Record payload (after PuzzleHeader, size = n):
//     given[n*n]     2 bits per cell (0 none, 1 light, 2 dark), low bits first
//     solution[n*n]  1 bit per cell (0 light, 1 dark)
#ifndef VEIN_H
#define VEIN_H

#include "puzzle_common.h"

#define VEIN_MIN_N 6
#define VEIN_MAX_N 8

enum VeinCell { VEIN_EMPTY = 0, VEIN_LIGHT = 1, VEIN_DARK = 2 };

typedef struct {
    uint8_t n;
    uint8_t given[PUZZLE_MAX_CELLS];      // enum VeinCell (VEIN_EMPTY = free cell)
    uint8_t solution[PUZZLE_MAX_CELLS];   // VEIN_LIGHT / VEIN_DARK
} VeinPuzzle;

typedef struct {
    uint8_t cell[PUZZLE_MAX_CELLS];       // enum VeinCell, givens included
} VeinBoard;

// --- rules -------------------------------------------------------------------
void vein_board_init(const VeinPuzzle *p, VeinBoard *b);     // givens only
// Flag every cell that takes part in a triple or in an over-full line;
// returns the number of flagged cells. `conflict` may be NULL.
int  vein_conflicts(const VeinPuzzle *p, const VeinBoard *b, uint8_t *conflict);
bool vein_solved(const VeinPuzzle *p, const VeinBoard *b);

// --- packing -----------------------------------------------------------------
int  vein_payload_len(int n);
int  vein_pack(const VeinPuzzle *p, uint8_t *out);
bool vein_unpack(const uint8_t *in, int n, VeinPuzzle *p);

// --- deduction solver ----------------------------------------------------------
enum VeinTechnique {
    VEINT_TRIPLE = 0,    // two alike neighbours (or a sandwich) force the other ore
    VEINT_COUNT,         // a line already holds n/2 of one ore: the rest is the other
    VEINT_TRIAL,         // one ore here would leave the line unable to finish
    VEINT_COUNT_TECH
};

typedef struct {
    int  steps[VEINT_COUNT_TECH];
    int  hardest;
    bool solved;
} VeinSolveStats;

void vein_deduce(const VeinPuzzle *p, const VeinBoard *start, VeinSolveStats *st, VeinBoard *out);
// One safe step for hints: fills `cell` with `value`. False when nothing is
// deducible or the board contradicts the solution.
bool vein_next_step(const VeinPuzzle *p, const VeinBoard *b, int *cell, int *value);
int  vein_count_solutions(const VeinPuzzle *p, int cap);
int  vein_difficulty(const VeinPuzzle *p, const VeinSolveStats *st);

#endif
