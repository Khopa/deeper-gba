// RNG, canonical forms, dedup set and bank writer shared by every generator.
#include <stdlib.h>
#include <string.h>
#include "gen.h"

// --- RNG (splitmix64) -----------------------------------------------------------------

void rng_init(Rng *r, uint64_t seed) { r->s = seed * 0x9E3779B97F4A7C15ull + 1; }

uint64_t rng_u64(Rng *r)
{
    uint64_t z = (r->s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

int rng_below(Rng *r, int n) { return (int)(rng_u64(r) % (uint64_t)n); }

void rng_shuffle(Rng *r, int *a, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = rng_below(r, i + 1), t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

// --- canonical forms --------------------------------------------------------------------

void canon_transform_cell(int n, int t, int r, int c, int *out_r, int *out_c)
{
    for (int k = 0; k < (t & 3); k++) {      // rotate 90° clockwise
        int nr = c, nc = n - 1 - r;
        r = nr;
        c = nc;
    }
    if (t & 4) c = n - 1 - c;                // mirror left-right
    *out_r = r;
    *out_c = c;
}

static void transform_grid(int n, int t, const uint8_t *in, uint8_t *out)
{
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            int tr, tc;
            canon_transform_cell(n, t, r, c, &tr, &tc);
            out[tr * n + tc] = in[r * n + c];
        }
}

static void relabel(int n, uint8_t *g)
{
    int map[256];
    memset(map, -1, sizeof map);
    int next = 0;
    for (int i = 0; i < n * n; i++) {
        if (map[g[i]] < 0) map[g[i]] = next++;
        g[i] = (uint8_t)map[g[i]];
    }
}

static int canon_generic(int n, const uint8_t *in, uint8_t *out, bool labels)
{
    uint8_t best[PUZZLE_MAX_CELLS], cur[PUZZLE_MAX_CELLS];
    int best_t = 0;
    for (int t = 0; t < 8; t++) {
        transform_grid(n, t, in, cur);
        if (labels) relabel(n, cur);
        if (t == 0 || memcmp(cur, best, n * n) < 0) {
            memcpy(best, cur, n * n);
            best_t = t;
        }
    }
    memcpy(out, best, n * n);
    return best_t;
}

int canon_labels(int n, const uint8_t *in, uint8_t *out) { return canon_generic(n, in, out, true); }
int canon_values(int n, const uint8_t *in, uint8_t *out) { return canon_generic(n, in, out, false); }

uint64_t fnv1a64(const void *data, int len)
{
    const uint8_t *p = data;
    uint64_t h = 0xCBF29CE484222325ull;
    for (int i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

// --- hash set -------------------------------------------------------------------------------

void hs_init(HashSet *h)
{
    h->cap = 1024;
    h->count = 0;
    h->keys = calloc(h->cap, sizeof *h->keys);
}

static void hs_grow(HashSet *h)
{
    uint64_t *old = h->keys;
    int old_cap = h->cap;
    h->cap *= 2;
    h->keys = calloc(h->cap, sizeof *h->keys);
    h->count = 0;
    for (int i = 0; i < old_cap; i++)
        if (old[i]) hs_insert(h, old[i]);
    free(old);
}

bool hs_insert(HashSet *h, uint64_t key)
{
    if (!key) key = 1;                       // 0 marks an empty slot
    if (h->count * 2 >= h->cap) hs_grow(h);
    uint64_t i = key & (uint64_t)(h->cap - 1);
    while (h->keys[i]) {
        if (h->keys[i] == key) return false;
        i = (i + 1) & (uint64_t)(h->cap - 1);
    }
    h->keys[i] = key;
    h->count++;
    return true;
}

void hs_free(HashSet *h) { free(h->keys); }

// --- bank -------------------------------------------------------------------------------------
// File layout (little endian), see docs/puzzle_bank.md:
//     0  "DPZ1"
//     4  u8 family, u8 reserved, u16 count
//     8  u16 diff_start[12]   first record index whose difficulty >= d (d = 0..11)
//    32  u32 offset[count]    byte offset of each record from the start of the file
//        records: PuzzleHeader (4 bytes) + payload

void gbank_init(GenBank *b)
{
    b->cap = 256;
    b->count = 0;
    b->rec = malloc(b->cap * sizeof *b->rec);
}

void gbank_add(GenBank *b, const GenRecord *r)
{
    if (b->count == b->cap) {
        b->cap *= 2;
        b->rec = realloc(b->rec, b->cap * sizeof *b->rec);
    }
    b->rec[b->count++] = *r;
}

static int cmp_diff(const void *a, const void *b)
{
    const GenRecord *x = a, *y = b;
    if (x->hdr.difficulty != y->hdr.difficulty) return x->hdr.difficulty - y->hdr.difficulty;
    if (x->hdr.size != y->hdr.size) return x->hdr.size - y->hdr.size;
    return (x->canon_key > y->canon_key) - (x->canon_key < y->canon_key);   // stable, seed-independent order
}

void gbank_sort_by_difficulty(GenBank *b) { qsort(b->rec, b->count, sizeof *b->rec, cmp_diff); }

static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE *f, unsigned v) { put16(f, v & 0xFFFF); put16(f, v >> 16); }

bool gbank_write(const GenBank *b, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fwrite("DPZ1", 1, 4, f);
    fputc(b->count ? b->rec[0].hdr.family : 0, f);
    fputc(0, f);
    put16(f, b->count);
    for (int d = 0; d < 12; d++) {
        int i = 0;
        while (i < b->count && b->rec[i].hdr.difficulty < d) i++;
        put16(f, i);
    }
    unsigned off = 32 + 4 * b->count;
    for (int i = 0; i < b->count; i++) {
        put32(f, off);
        off += PUZZLE_HEADER_LEN + b->rec[i].payload_len;
    }
    for (int i = 0; i < b->count; i++) {
        fwrite(&b->rec[i].hdr, 1, PUZZLE_HEADER_LEN, f);
        fwrite(b->rec[i].payload, 1, b->rec[i].payload_len, f);
    }
    fclose(f);
    return true;
}

void gbank_free(GenBank *b) { free(b->rec); }
