// BLOCK — filling the cavity.
//
// Inside an n x n area (n = 5..7) a cavity of open cells must be filled
// exactly with a given set of stone blocks (polyominoes of 3 to 5 cells),
// each used once, rotated or flipped as needed, without overlaps.
//
// Record payload (after PuzzleHeader, size = n):
//     cavity[n*n]   1 bit per cell (1 = open), low bit first, (n*n+7)/8 bytes
//     count         1 byte
//     piece[count]  2 bytes each: shape (5 bits) | orient (3 bits), anchor cell
//                   (the solution placement; anchor = top-left cell of the
//                   oriented shape's bounding box)
#ifndef BLOCK_H
#define BLOCK_H

#include "puzzle_common.h"

#define BLOCK_MIN_N 5
#define BLOCK_MAX_N 7
#define BLOCK_MAX_PIECES 12
#define BLOCK_MAX_SIZE 5
#define BLOCK_SHAPES 19          // 2 trominoes, 5 tetrominoes, 12 pentominoes

typedef struct {
    uint8_t size;
    uint8_t r[BLOCK_MAX_SIZE], c[BLOCK_MAX_SIZE];   // cells relative to the bounding box origin
    uint8_t h, w;
} BlockShape;

// Shape `shape` in orientation `orient` (0..7: rotations, then mirrored rotations)
void block_shape_get(int shape, int orient, BlockShape *out);
// Number of distinct orientations of a shape (1, 2, 4 or 8)
int  block_shape_orients(int shape);
// The same, as a table (read by the emulator scenarios; tests check it matches)
extern const uint8_t block_orient_count[BLOCK_SHAPES];
// Identify a set of cells (relative coordinates) as (shape, orient); false if not in the catalogue
bool block_shape_identify(const BlockShape *cells, int *shape, int *orient);

typedef struct {
    uint8_t n;
    uint8_t count;
    uint8_t cavity[PUZZLE_MAX_CELLS];      // 1 = open
    uint8_t shape[BLOCK_MAX_PIECES];
    uint8_t sol_orient[BLOCK_MAX_PIECES];
    uint8_t sol_anchor[BLOCK_MAX_PIECES];
    uint8_t open_cells;
} BlockPuzzle;

typedef struct {
    uint8_t placed[BLOCK_MAX_PIECES];      // 1 when the piece is on the board
    uint8_t orient[BLOCK_MAX_PIECES];
    uint8_t anchor[BLOCK_MAX_PIECES];
} BlockBoard;

// --- rules -----------------------------------------------------------------------
void block_board_init(BlockBoard *b);
// Piece index covering `cell` on the board, -1 if none
int  block_piece_at(const BlockPuzzle *p, const BlockBoard *b, int cell);
// Can piece `piece` sit at (orient, anchor)? Inside the cavity and free of other pieces.
bool block_fits(const BlockPuzzle *p, const BlockBoard *b, int piece, int orient, int anchor);
bool block_solved(const BlockPuzzle *p, const BlockBoard *b);
// Cells of a placed piece (returns count, fills cells[])
int  block_piece_cells(const BlockPuzzle *p, int piece, int orient, int anchor, int *cells);

// --- packing ---------------------------------------------------------------------
int  block_payload_len(const BlockPuzzle *p);
int  block_pack(const BlockPuzzle *p, uint8_t *out);
bool block_unpack(const uint8_t *in, int n, int max_len, BlockPuzzle *p);

// --- solving -----------------------------------------------------------------------
// Number of distinct tilings (capped); identical shapes are interchangeable.
int  block_count_solutions(const BlockPuzzle *p, int cap, long *effort);
// Hint: a piece still to place whose solution spot is free. Returns true and
// (piece, orient, anchor); false with *piece = index of a misplaced piece
// (or -1 when nothing is left).
bool block_next_step(const BlockPuzzle *p, const BlockBoard *b, int *piece, int *orient, int *anchor);
int  block_difficulty(const BlockPuzzle *p, long effort);

#endif
