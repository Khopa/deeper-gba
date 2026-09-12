// TUNNEL — one continuous gallery.
//
// On an n x n grid (n = 5..7) some cells are solid rock. The gallery must
// start at exit 1, pass through every open cell exactly once (orthogonal
// moves), visit the numbered exits in increasing order and end at the last
// one.
//
// Record payload (after PuzzleHeader, size = n):
//     cell[n*n]      4 bits per cell: 0 open, 1..k exit number, 15 solid rock
//     path[n*n-1]    2 bits per step (0 N, 1 E, 2 S, 3 W) from exit 1; unused
//                    trailing steps are 0
#ifndef TUNNEL_H
#define TUNNEL_H

#include "puzzle_common.h"

#define TUNNEL_MIN_N 5
#define TUNNEL_MAX_N 7
#define TUNNEL_ROCK  15
#define TUNNEL_MAX_EXITS 12

enum { DIR_N = 0, DIR_E, DIR_S, DIR_W };

typedef struct {
    uint8_t n;
    uint8_t cell[PUZZLE_MAX_CELLS];     // 0 open, 1..k exit, TUNNEL_ROCK
    uint8_t exits;                      // k
    uint8_t open_cells;                 // cells the path must cover
    uint8_t path[PUZZLE_MAX_CELLS];     // solution: cell indices in order (open_cells entries)
} TunnelPuzzle;

typedef struct {
    uint8_t len;                        // cells in the drawn path (0 = nothing yet)
    uint8_t path[PUZZLE_MAX_CELLS];     // cell indices in order
} TunnelBoard;

// --- rules ---------------------------------------------------------------------
void tunnel_board_init(const TunnelPuzzle *p, TunnelBoard *b);   // path = [exit 1]
// Can the path be extended to `cell`? Fails on non-adjacent, rock, revisit
// or an exit out of order.
bool tunnel_can_extend(const TunnelPuzzle *p, const TunnelBoard *b, int cell);
void tunnel_extend(TunnelBoard *b, int cell);
void tunnel_truncate(TunnelBoard *b, int len);                   // keep the first `len` cells
bool tunnel_solved(const TunnelPuzzle *p, const TunnelBoard *b);
int  tunnel_next_exit(const TunnelPuzzle *p, const TunnelBoard *b);   // exit number expected next
// Connection bits of a path cell (1 N, 2 E, 4 S, 8 W), 0 if not on the path
int  tunnel_links(const TunnelPuzzle *p, const TunnelBoard *b, int cell);
int  tunnel_step_dir(int n, int from, int to);                    // DIR_* or -1

// --- packing -------------------------------------------------------------------
int  tunnel_payload_len(int n);
int  tunnel_pack(const TunnelPuzzle *p, uint8_t *out);
bool tunnel_unpack(const uint8_t *in, int n, TunnelPuzzle *p);

// --- solving ---------------------------------------------------------------------
// Number of valid galleries (capped). `effort` (may be NULL) receives the
// number of search nodes visited: the difficulty proxy for this family.
int  tunnel_count_solutions(const TunnelPuzzle *p, int cap, long *effort);
// Hint: the next cell of the stored solution if the drawn path is a prefix
// of it; otherwise returns false and sets *cell to the first wrong index.
bool tunnel_next_step(const TunnelPuzzle *p, const TunnelBoard *b, int *cell);
int  tunnel_difficulty(const TunnelPuzzle *p, long effort);

#endif
