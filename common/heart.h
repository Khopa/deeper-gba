// HEART — the core's sounding picture (the last room of every descent).
//
// An n x n grid (10, 12 or 15) hides a picture drawn in ore. The clues of a
// row or column list, in order, the lengths of its ore runs; the player marks
// ore (and rock, as a note) until the picture is complete. Every picture kept
// in the bank is solvable line by line (so it has one solution); the generator
// pre-reveals the few cells needed when a drawing is not.
//
// Record payload (after PuzzleHeader, size = n):
//     picture[n*n]   1 bit per cell (1 = ore), low bits first
//     given[n*n]     1 bit per cell (1 = revealed from the start)
#ifndef HEART_H
#define HEART_H

#include "puzzle_common.h"

#define HEART_MIN_N     10
#define HEART_MAX_N     15
#define HEART_MAX_CELLS (HEART_MAX_N * HEART_MAX_N)
#define HEART_MAX_CLUES 8                    // runs a line can hold at n = 15
#define HEART_SIZE_COUNT 3

enum HeartCell { HEART_UNKNOWN = 0, HEART_ORE = 1, HEART_ROCK = 2 };

// The picture size of each descent length (15, 30, 60 layers) and the
// difficulty label the run builder asks the bank for: spaced so a window of
// +-1 around one never reaches the next size.
extern const uint8_t heart_sizes[HEART_SIZE_COUNT];
int heart_difficulty(int n);

// Screen layout limits (source/room.c draws 8 px cells): row clues fit in
// HEART_ROW_CLUE_CHARS characters ("1 2 10"), column clues in a few rows of
// 8 px (a two-digit clue stacks on two rows).
#define HEART_ROW_CLUE_CHARS 7
int heart_col_clue_rows(int n);              // rows available above the grid

typedef struct {
    uint8_t n;
    uint8_t picture[HEART_MAX_CELLS];        // 1 = ore
    uint8_t given[HEART_MAX_CELLS];          // 1 = shown from the start
} HeartPuzzle;

typedef struct {
    uint8_t cell[HEART_MAX_CELLS];           // enum HeartCell
} HeartBoard;

// --- rules -------------------------------------------------------------------
void heart_board_init(const HeartPuzzle *p, HeartBoard *b);   // givens only
// Clues of a line: 0..n-1 rows, n..2n-1 columns. Returns the count; an empty
// line yields one clue of 0.
int  heart_line_clues(const HeartPuzzle *p, int line, uint8_t *out);
// True when the ore marked on the line forms exactly the clued runs
bool heart_line_done(const HeartPuzzle *p, const HeartBoard *b, int line);
// Ore marked where the picture has rock: flags them, returns the count
int  heart_conflicts(const HeartPuzzle *p, const HeartBoard *b, uint8_t *conflict);
bool heart_solved(const HeartPuzzle *p, const HeartBoard *b);
// Do the clues fit the room layout? (rejects busy drawings at generation)
bool heart_fits_layout(const HeartPuzzle *p);

// --- packing -----------------------------------------------------------------
int  heart_payload_len(int n);
int  heart_pack(const HeartPuzzle *p, uint8_t *out);
bool heart_unpack(const uint8_t *in, int n, HeartPuzzle *p);

// --- line solver ---------------------------------------------------------------
// Settle every cell of one line that all placements of its runs agree on.
// `cells` (length n, enum HeartCell) is read and completed; false when the
// known cells contradict the clues.
bool heart_line_solve(int n, const uint8_t *clues, int count, uint8_t *cells);
// Line-by-line propagation to a fixpoint from the board (givens included).
// Returns true when every cell is settled; `passes` (may be NULL) counts the
// sweeps needed.
bool heart_deduce(const HeartPuzzle *p, HeartBoard *b, int *passes);

#endif
