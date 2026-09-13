// HEART generator: the pictures are drawings (assets/heart/*.png, turned into
// build/gen/heart_pictures.h by tools/heart_pictures.py), not random grids.
// Each attempt takes one drawing of the requested size, checks its clues fit
// the room layout, then pre-reveals the fewest cells (greedily, the one that
// settles the most cells first) until the line solver finishes the picture
// on its own — which is what makes the solution unique.
#include <string.h>
#include "gen.h"
#include "heart.h"
#include "heart_pictures.h"

static bool warned[HEART_PICTURE_COUNT];

static void load_picture(const HeartPicture *pic, HeartPuzzle *p)
{
    memset(p, 0, sizeof *p);
    p->n = (uint8_t)pic->n;
    for (int r = 0; r < pic->n; r++)
        for (int c = 0; c < pic->n; c++) {
            char ch = pic->rows[r][c];
            p->picture[cell_at(pic->n, r, c)] = (uint8_t)(ch == '#' || ch == 'X' || ch == '1');
        }
}

static int settled(const HeartPuzzle *p, const HeartBoard *b)
{
    int count = 0;
    for (int i = 0; i < p->n * p->n; i++) count += b->cell[i] != HEART_UNKNOWN;
    return count;
}

// Reveal cells until the line solver completes the picture; returns the givens added
static int choose_givens(HeartPuzzle *p)
{
    HeartBoard b;
    heart_board_init(p, &b);
    int added = 0;
    while (!heart_deduce(p, &b, NULL)) {
        int best = -1, best_score = -1;
        for (int i = 0; i < p->n * p->n; i++) {
            if (b.cell[i] != HEART_UNKNOWN) continue;
            HeartBoard t = b;
            t.cell[i] = p->picture[i] ? HEART_ORE : HEART_ROCK;
            heart_deduce(p, &t, NULL);
            int score = settled(p, &t);
            if (score > best_score) { best_score = score; best = i; }
        }
        if (best < 0) break;
        p->given[best] = 1;
        b.cell[best] = p->picture[best] ? HEART_ORE : HEART_ROCK;
        added++;
    }
    return added;
}

static bool heart_attempt(Rng *rng, int n, HashSet *seen, GenRecord *out)
{
    int idx[HEART_PICTURE_COUNT], k = 0;
    for (int i = 0; i < HEART_PICTURE_COUNT; i++) if (heart_pictures[i].n == n) idx[k++] = i;
    if (!k) return false;
    int which = idx[rng_below(rng, k)];
    const HeartPicture *pic = &heart_pictures[which];

    HeartPuzzle p;
    load_picture(pic, &p);
    if (!heart_fits_layout(&p)) {
        if (!warned[which]) {
            warned[which] = true;
            fprintf(stderr, "heart: %s (%dx%d) skipped: a line has too many runs for the screen\n", pic->name, n, n);
        }
        return false;
    }
    uint64_t key = fnv1a64(p.picture, n * n) ^ ((uint64_t)n << 56) ^ ((uint64_t)FAM_HEART << 48);
    if (!hs_insert(seen, key)) return false;

    int givens = choose_givens(&p);
    HeartBoard check;
    heart_board_init(&p, &check);
    if (!heart_deduce(&p, &check, NULL)) return false;     // cannot happen: the loop above ensures it

    memset(out, 0, sizeof *out);
    out->hdr.family = FAM_HEART;
    out->hdr.size = (uint8_t)n;
    out->hdr.difficulty = (uint8_t)heart_difficulty(n);
    out->hdr.flags = (uint8_t)(givens > 255 ? 255 : givens);
    out->payload_len = heart_pack(&p, out->payload);
    out->canon_key = key;
    return true;
}

const FamilyGen gen_heart = { "heart", FAM_HEART, HEART_MIN_N, HEART_MAX_N, heart_attempt };
