// LEDGER — the prospector's notebook.
//
// An n x n grid (n = 4, 6 or 8) is split into boxes of 2 rows x n/2 columns.
// Every ore symbol 1..n must appear exactly once per row, column and box.
// Some cells are given.
//
// Record payload (after PuzzleHeader, size = n):
//     given[n*n]     4 bits per cell (0 = free), low nibble first
//     solution[n*n]  4 bits per cell
#ifndef LEDGER_H
#define LEDGER_H

#include "puzzle_common.h"

#define LEDGER_MIN_N 4
#define LEDGER_MAX_N 8
#define LEDGER_BOX_H 2

typedef struct {
    uint8_t n;
    uint8_t given[PUZZLE_MAX_CELLS];
    uint8_t solution[PUZZLE_MAX_CELLS];
} LedgerPuzzle;

typedef struct {
    uint8_t cell[PUZZLE_MAX_CELLS];      // 0 empty, else 1..n (givens included)
} LedgerBoard;

static inline int ledger_box_w(int n) { return n / LEDGER_BOX_H; }
static inline int ledger_box_of(int n, int r, int c) { return (r / LEDGER_BOX_H) * LEDGER_BOX_H + c / ledger_box_w(n); }

// --- rules -------------------------------------------------------------------
void ledger_board_init(const LedgerPuzzle *p, LedgerBoard *b);
int  ledger_conflicts(const LedgerPuzzle *p, const LedgerBoard *b, uint8_t *conflict);   // duplicates flagged
bool ledger_solved(const LedgerPuzzle *p, const LedgerBoard *b);

// --- packing -----------------------------------------------------------------
int  ledger_payload_len(int n);
int  ledger_pack(const LedgerPuzzle *p, uint8_t *out);
bool ledger_unpack(const uint8_t *in, int n, LedgerPuzzle *p);

// --- deduction solver ----------------------------------------------------------
enum LedgerTechnique {
    LEDT_NAKED = 0,      // a cell has a single candidate left
    LEDT_HIDDEN,         // a symbol has a single place left in a unit
    LEDT_LOCKED,         // a symbol confined to one line inside a box (or one box on a line)
    LEDT_COUNT
};

typedef struct {
    int  steps[LEDT_COUNT];
    int  hardest;
    bool solved;
} LedgerSolveStats;

void ledger_deduce(const LedgerPuzzle *p, const LedgerBoard *start, LedgerSolveStats *st, LedgerBoard *out);
bool ledger_next_step(const LedgerPuzzle *p, const LedgerBoard *b, int *cell, int *value);
int  ledger_count_solutions(const LedgerPuzzle *p, int cap);
int  ledger_difficulty(const LedgerPuzzle *p, const LedgerSolveStats *st);

#endif
