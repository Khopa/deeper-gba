// BLOCK rules, packing and exact-cover search. See block.h.
#include "block.h"

// Shape catalogue: '#' cells, rows separated by '/'.
static const char *const shape_art[BLOCK_SHAPES] = {
    "###", "##/#.",                                   // trominoes
    "####", "##/##", "###/.#.", "##./.##", "###/#..", // tetrominoes I O T S L
    ".##/##./.#.", "#####", "####/#...", ".###/##..", "##/##/#.", "###/.#./.#.",
    "#.#/###", "#../#../###", "#../##./.##", ".#./###/.#.", "####/.#..", "##./.#./.##",
};                                                    // pentominoes F I L N P T U V W X Y Z

static void base_shape(int shape, BlockShape *out)
{
    memset(out, 0, sizeof *out);
    int r = 0, c = 0;
    for (const char *s = shape_art[shape]; *s; s++) {
        if (*s == '/') { r++; c = 0; continue; }
        if (*s == '#') {
            out->r[out->size] = (uint8_t)r;
            out->c[out->size] = (uint8_t)c;
            out->size++;
        }
        c++;
    }
}

// Sort cells row-major and shift them to the origin.
static void normalize(BlockShape *s)
{
    int minr = 99, minc = 99;
    for (int i = 0; i < s->size; i++) { if (s->r[i] < minr) minr = s->r[i]; if (s->c[i] < minc) minc = s->c[i]; }
    int maxr = 0, maxc = 0;
    for (int i = 0; i < s->size; i++) {
        s->r[i] = (uint8_t)(s->r[i] - minr);
        s->c[i] = (uint8_t)(s->c[i] - minc);
        if (s->r[i] > maxr) maxr = s->r[i];
        if (s->c[i] > maxc) maxc = s->c[i];
    }
    s->h = (uint8_t)(maxr + 1);
    s->w = (uint8_t)(maxc + 1);
    for (int i = 1; i < s->size; i++)
        for (int j = i; j > 0 && (s->r[j] < s->r[j - 1] || (s->r[j] == s->r[j - 1] && s->c[j] < s->c[j - 1])); j--) {
            uint8_t t = s->r[j]; s->r[j] = s->r[j - 1]; s->r[j - 1] = t;
            t = s->c[j]; s->c[j] = s->c[j - 1]; s->c[j - 1] = t;
        }
}

static void transform(const BlockShape *in, int t, BlockShape *out)
{
    *out = *in;
    for (int i = 0; i < in->size; i++) {
        int r = in->r[i], c = in->c[i];
        for (int k = 0; k < (t & 3); k++) { int nr = c, nc = 7 - r; r = nr; c = nc; }
        if (t & 4) c = 7 - c;
        out->r[i] = (uint8_t)r;
        out->c[i] = (uint8_t)c;
    }
    normalize(out);
}

static bool same_shape(const BlockShape *a, const BlockShape *b)
{
    if (a->size != b->size) return false;
    for (int i = 0; i < a->size; i++) if (a->r[i] != b->r[i] || a->c[i] != b->c[i]) return false;
    return true;
}

// Distinct orientations of a shape, in transform order.
static int orientations(int shape, BlockShape *out, int max)
{
    BlockShape base;
    base_shape(shape, &base);
    int n = 0;
    for (int t = 0; t < 8 && n < max; t++) {
        BlockShape s;
        transform(&base, t, &s);
        bool dup = false;
        for (int i = 0; i < n && !dup; i++) dup = same_shape(&out[i], &s);
        if (!dup) out[n++] = s;
    }
    return n;
}

int block_shape_orients(int shape)
{
    BlockShape all[8];
    return orientations(shape, all, 8);
}

void block_shape_get(int shape, int orient, BlockShape *out)
{
    BlockShape all[8];
    int n = orientations(shape, all, 8);
    *out = all[orient % n];
}

bool block_shape_identify(const BlockShape *cells, int *shape, int *orient)
{
    BlockShape norm = *cells;
    normalize(&norm);
    for (int s = 0; s < BLOCK_SHAPES; s++) {
        BlockShape all[8];
        int n = orientations(s, all, 8);
        for (int o = 0; o < n; o++)
            if (same_shape(&all[o], &norm)) { *shape = s; *orient = o; return true; }
    }
    return false;
}

// --- rules -----------------------------------------------------------------------------

void block_board_init(BlockBoard *b) { memset(b, 0, sizeof *b); }

int block_piece_cells(const BlockPuzzle *p, int piece, int orient, int anchor, int *cells)
{
    BlockShape s;
    block_shape_get(p->shape[piece], orient, &s);
    int n = p->n, ar = anchor / n, ac = anchor % n;
    for (int i = 0; i < s.size; i++) {
        int r = ar + s.r[i], c = ac + s.c[i];
        if (r >= n || c >= n) return 0;                    // sticks out of the area
        cells[i] = cell_at(n, r, c);
    }
    return s.size;
}

int block_piece_at(const BlockPuzzle *p, const BlockBoard *b, int cell)
{
    for (int k = 0; k < p->count; k++) {
        if (!b->placed[k]) continue;
        int cells[BLOCK_MAX_SIZE];
        int m = block_piece_cells(p, k, b->orient[k], b->anchor[k], cells);
        for (int i = 0; i < m; i++) if (cells[i] == cell) return k;
    }
    return -1;
}

bool block_fits(const BlockPuzzle *p, const BlockBoard *b, int piece, int orient, int anchor)
{
    int cells[BLOCK_MAX_SIZE];
    int m = block_piece_cells(p, piece, orient, anchor, cells);
    if (!m) return false;
    for (int i = 0; i < m; i++) {
        if (!p->cavity[cells[i]]) return false;
        int other = block_piece_at(p, b, cells[i]);
        if (other >= 0 && other != piece) return false;
    }
    return true;
}

bool block_solved(const BlockPuzzle *p, const BlockBoard *b)
{
    for (int k = 0; k < p->count; k++) if (!b->placed[k]) return false;
    int cells = p->n * p->n;
    for (int i = 0; i < cells; i++)
        if (p->cavity[i] && block_piece_at(p, b, i) < 0) return false;
    return true;
}

// --- packing -----------------------------------------------------------------------------

int block_payload_len(const BlockPuzzle *p) { return (p->n * p->n + 7) / 8 + 1 + 2 * p->count; }

int block_pack(const BlockPuzzle *p, uint8_t *out)
{
    int n = p->n, cells = n * n, mask_len = (cells + 7) / 8;
    memset(out, 0, block_payload_len(p));
    for (int i = 0; i < cells; i++) if (p->cavity[i]) out[i >> 3] |= (uint8_t)(1 << (i & 7));
    out[mask_len] = p->count;
    for (int k = 0; k < p->count; k++) {
        out[mask_len + 1 + 2 * k] = (uint8_t)((p->shape[k] << 3) | (p->sol_orient[k] & 7));
        out[mask_len + 2 + 2 * k] = p->sol_anchor[k];
    }
    return block_payload_len(p);
}

bool block_unpack(const uint8_t *in, int n, int max_len, BlockPuzzle *p)
{
    if (n < BLOCK_MIN_N || n > BLOCK_MAX_N) return false;
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)n;
    int cells = n * n, mask_len = (cells + 7) / 8;
    if (max_len < mask_len + 1) return false;
    for (int i = 0; i < cells; i++) {
        p->cavity[i] = (in[i >> 3] >> (i & 7)) & 1;
        p->open_cells += p->cavity[i];
    }
    p->count = in[mask_len];
    if (p->count < 1 || p->count > BLOCK_MAX_PIECES || max_len < block_payload_len(p)) return false;
    BlockBoard sol;
    block_board_init(&sol);
    int covered = 0;
    for (int k = 0; k < p->count; k++) {
        int sh = in[mask_len + 1 + 2 * k] >> 3, o = in[mask_len + 1 + 2 * k] & 7;
        if (sh >= BLOCK_SHAPES || o >= block_shape_orients(sh)) return false;
        p->shape[k] = (uint8_t)sh;
        p->sol_orient[k] = (uint8_t)o;
        p->sol_anchor[k] = in[mask_len + 2 + 2 * k];
        if (p->sol_anchor[k] >= cells || !block_fits(p, &sol, k, o, p->sol_anchor[k])) return false;
        sol.placed[k] = 1;
        sol.orient[k] = (uint8_t)o;
        sol.anchor[k] = p->sol_anchor[k];
        BlockShape s;
        block_shape_get(sh, o, &s);
        covered += s.size;
    }
    return covered == p->open_cells && block_solved(p, &sol);
}

// --- search ---------------------------------------------------------------------------------

typedef struct {
    const BlockPuzzle *p;
    int cap, found;
    long nodes;
    uint8_t covered[PUZZLE_MAX_CELLS];
    uint8_t used[BLOCK_MAX_PIECES];
} BSearch;

#define BLOCK_NODE_LIMIT 200000L

static void search_rec(BSearch *s, int remaining)
{
    if (s->found >= s->cap) return;
    if (++s->nodes > BLOCK_NODE_LIMIT) { s->found = s->cap; return; }
    if (!remaining) { s->found++; return; }
    const BlockPuzzle *p = s->p;
    int n = p->n, cells = n * n;
    int target = -1;
    for (int i = 0; i < cells && target < 0; i++) if (p->cavity[i] && !s->covered[i]) target = i;
    int tr = target / n, tc = target % n;
    for (int k = 0; k < p->count; k++) {
        if (s->used[k]) continue;
        bool twin_before = false;                 // identical shapes: only the first unused one tries
        for (int j = 0; j < k && !twin_before; j++) twin_before = !s->used[j] && p->shape[j] == p->shape[k];
        if (twin_before) continue;
        BlockShape all[8];
        int no = orientations(p->shape[k], all, 8);
        for (int o = 0; o < no; o++) {
            const BlockShape *sh = &all[o];
            // the target must be the shape's first cell (top-left in row-major order)
            int ar = tr - sh->r[0], ac = tc - sh->c[0];
            if (ar < 0 || ac < 0 || ar + sh->h > n || ac + sh->w > n) continue;
            bool ok = true;
            int cov[BLOCK_MAX_SIZE];
            for (int i = 0; i < sh->size && ok; i++) {
                cov[i] = cell_at(n, ar + sh->r[i], ac + sh->c[i]);
                ok = p->cavity[cov[i]] && !s->covered[cov[i]];
            }
            if (!ok) continue;
            for (int i = 0; i < sh->size; i++) s->covered[cov[i]] = 1;
            s->used[k] = 1;
            search_rec(s, remaining - sh->size);
            s->used[k] = 0;
            for (int i = 0; i < sh->size; i++) s->covered[cov[i]] = 0;
            if (s->found >= s->cap) return;
        }
    }
}

int block_count_solutions(const BlockPuzzle *p, int cap, long *effort)
{
    BSearch s;
    memset(&s, 0, sizeof s);
    s.p = p;
    s.cap = cap;
    search_rec(&s, p->open_cells);
    if (effort) *effort = s.nodes;
    return s.found;
}

bool block_next_step(const BlockPuzzle *p, const BlockBoard *b, int *piece, int *orient, int *anchor)
{
    // every placed piece must sit exactly where some identical piece sits in the solution
    for (int k = 0; k < p->count; k++) {
        if (!b->placed[k]) continue;
        int mine[BLOCK_MAX_SIZE], theirs[BLOCK_MAX_SIZE];
        int m = block_piece_cells(p, k, b->orient[k], b->anchor[k], mine);
        bool matched = false;
        for (int j = 0; j < p->count && !matched; j++) {
            if (p->shape[j] != p->shape[k]) continue;
            int t = block_piece_cells(p, j, p->sol_orient[j], p->sol_anchor[j], theirs);
            if (t != m) continue;
            int same = 0;
            for (int a = 0; a < m; a++) for (int c = 0; c < t; c++) same += mine[a] == theirs[c];
            matched = same == m;
        }
        if (!matched) { *piece = k; return false; }
    }
    // place the first unplaced piece whose solution cells are free
    for (int j = 0; j < p->count; j++) {
        int theirs[BLOCK_MAX_SIZE];
        int t = block_piece_cells(p, j, p->sol_orient[j], p->sol_anchor[j], theirs);
        bool free_ = true;
        for (int a = 0; a < t && free_; a++) free_ = block_piece_at(p, b, theirs[a]) < 0;
        if (!free_) continue;
        for (int k = 0; k < p->count; k++)
            if (!b->placed[k] && p->shape[k] == p->shape[j]) {
                *piece = k;
                *orient = p->sol_orient[j];
                *anchor = p->sol_anchor[j];
                return true;
            }
    }
    *piece = -1;
    return false;
}

// --- difficulty -------------------------------------------------------------------------------

int block_difficulty(const BlockPuzzle *p, long effort)
{
    // Search effort (nodes of the exact-cover search) is the difficulty proxy;
    // many blocks add bookkeeping on top.
    static const long band[] = { 15, 30, 60, 120, 250, 500, 1000, 2500 };
    int d = 1;
    for (int i = 0; i < (int)(sizeof band / sizeof band[0]); i++) if (effort > band[i]) d++;
    if (p->count >= 8) d++;
    return clamp_difficulty(d);
}
