// Shared definitions for every puzzle family. This header (and everything in
// common/) is compiled three times: in the ROM, in the PC generator and in
// the host unit tests, so it only depends on <stdint.h>/<stdbool.h>.
#ifndef PUZZLE_COMMON_H
#define PUZZLE_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Puzzle families. Player-facing names live in the engine's string tables;
// these identifiers never mention the puzzles' real-world origins.
enum PuzzleFamily {
    FAM_DIG    = 0,   // one dig per row/column/rock region, never adjacent (signature puzzle)
    FAM_VEIN   = 1,   // two ore types, balanced rows/columns, no three in a row
    FAM_BLOCK  = 2,   // fit stone blocks exactly into a cavity
    FAM_TUNNEL = 3,   // one continuous gallery through every cell, exits in order
    FAM_LEDGER = 4,   // each ore symbol once per row/column/zone
    FAM_NUGGET = 5,   // bonus room, generated at run time (no bank)
    FAM_HEART  = 6,   // the core only: a picture hidden in ore, clued by run lengths
    FAM_COUNT
};

#define PUZZLE_MAX_N     8            // largest grid side of the 16 px families
#define PUZZLE_MAX_CELLS (PUZZLE_MAX_N * PUZZLE_MAX_N)
#define ROOM_MAX_CELLS   225          // largest grid of any family (HEART, 15 x 15)

// Difficulty is a 1..10 scale shared by all families so the run builder can
// mix them on one curve. 0 means "not measured".
#define DIFF_MIN 1
#define DIFF_MAX 10
static inline int clamp_difficulty(int d) { return d < DIFF_MIN ? DIFF_MIN : d > DIFF_MAX ? DIFF_MAX : d; }

// Common 4-byte record header, followed by the family payload.
typedef struct {
    uint8_t family;      // enum PuzzleFamily
    uint8_t size;        // grid side (or family-specific main dimension)
    uint8_t difficulty;  // 1..10
    uint8_t flags;       // family-specific
} PuzzleHeader;

#define PUZZLE_HEADER_LEN 4

// Row-major cell index helpers
static inline int cell_at(int n, int r, int c) { return r * n + c; }

// Bit helpers for candidate masks (one uint64 per grid, cell i = bit i)
static inline uint64_t bit64(int i) { return (uint64_t)1 << i; }
static inline int popcount64(uint64_t x)
{
    int n = 0;
    while (x) { x &= x - 1; n++; }
    return n;
}
static inline int ctz64(uint64_t x)
{
    int n = 0;
    if (!x) return 64;
    while (!(x & 1)) { x >>= 1; n++; }
    return n;
}

#endif
