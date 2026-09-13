// NUGGET bonus room: prospecting for nuggets hidden in a 6x6 rock face.
//   A: break a cell. A nugget is collected (ore); bare rock shows how many
//      nuggets touch it (8 neighbours). Each miss uses one of the room's
//      chances (the stability bar); the room ends when they run out or when
//      every nugget is found, and always pays what was collected.
//   B: mark a cell as promising.
// There is no bank: the layout comes from a seed (run seed ^ depth) so a
// saved room reopens identical.
#include "puzzle.h"
#include "render.h"
#include "lang.h"

#define NUG_N        6
#define NUG_COUNT    6
#define NUG_ORE_EACH 8
#define NUG_ALL_BONUS 20

static uint8_t nugget[NUG_N * NUG_N];      // 1 = nugget here
static uint8_t state[NUG_N * NUG_N];       // 0 hidden, 1 revealed, 2 marked
static int collected;
static uint32_t seed;

static uint32_t next_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return *s = x;
}

static void layout_from_seed(uint32_t s)
{
    memset(nugget, 0, sizeof nugget);
    if (!s) s = 0x9E3779B9u;
    int placed = 0;
    while (placed < NUG_COUNT) {
        int i = (int)(next_rand(&s) % (NUG_N * NUG_N));
        if (!nugget[i]) { nugget[i] = 1; placed++; }
    }
}

static bool nugget_load(const PuzzleHeader *h, const uint8_t *payload)
{
    if (h->family != FAM_NUGGET) return false;
    seed = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) | ((uint32_t)payload[3] << 24);
    layout_from_seed(seed);
    memset(state, 0, sizeof state);
    collected = 0;
    return true;
}

static int nugget_size(void) { return NUG_N; }

static int neighbours(int i)
{
    int r = i / NUG_N, c = i % NUG_N, k = 0;
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
            if (!dr && !dc) continue;
            int rr = r + dr, cc = c + dc;
            if (rr >= 0 && rr < NUG_N && cc >= 0 && cc < NUG_N) k += nugget[cell_at(NUG_N, rr, cc)];
        }
    return k;
}

static void nugget_cell(int r, int c, CellView *out)
{
    int i = cell_at(NUG_N, r, c);
    out->conflict = 0;
    if (state[i] == 1) {
        out->variant = 1;
        out->edges = 0;
        out->pal = nugget[i] ? PAL_CELL_HILITE : PAL_REGION0 + 0;
        int k = neighbours(i);
        out->mark = nugget[i] ? MARK_GEM : k ? (u8)(MARK_DIGIT1 + k - 1) : MARK_NONE;
    } else {
        out->variant = 0;
        out->edges = 0;
        out->pal = PAL_REGION0 + 5;
        out->mark = state[i] == 2 ? MARK_CROSS : MARK_NONE;
    }
}

static bool all_found(void)
{
    for (int i = 0; i < NUG_N * NUG_N; i++) if (nugget[i] && state[i] != 1) return false;
    return true;
}

static ActionResult nugget_action(int r, int c, int act)
{
    ActionResult res = { false, false, false };
    int i = cell_at(NUG_N, r, c);
    if (state[i] == 1) return res;
    if (act == ACT_B) {
        state[i] = state[i] == 2 ? 0 : 2;
        res.changed = true;
        return res;
    }
    state[i] = 1;
    res.changed = true;
    if (nugget[i]) collected++;
    else res.mistake = true;                       // a miss spends a chance
    res.solved = all_found();
    return res;
}

static bool nugget_solved(void) { return all_found(); }

static int nugget_hint(int *r, int *c)
{
    for (int i = 0; i < NUG_N * NUG_N; i++)
        if (nugget[i] && state[i] == 0) {
            state[i] = 2;                          // mark a nugget as promising
            *r = i / NUG_N;
            *c = i % NUG_N;
            return HINT_APPLIED;
        }
    return HINT_NOTHING;
}

static int nugget_save(uint8_t *buf)
{
    memset(buf, 0, 1 + (NUG_N * NUG_N + 3) / 4);
    buf[0] = (uint8_t)collected;
    for (int i = 0; i < NUG_N * NUG_N; i++) buf[1 + (i >> 2)] |= (uint8_t)((state[i] & 3) << ((i & 3) * 2));
    return 1 + (NUG_N * NUG_N + 3) / 4;
}

static bool nugget_restore(const uint8_t *buf, int len)
{
    if (len < 1 + (NUG_N * NUG_N + 3) / 4) return false;
    collected = 0;
    for (int i = 0; i < NUG_N * NUG_N; i++) {
        int v = (buf[1 + (i >> 2)] >> ((i & 3) * 2)) & 3;
        if (v > 2) return false;
        state[i] = (uint8_t)v;
        if (v == 1 && nugget[i]) collected++;
    }
    return buf[0] == collected;
}

static int nugget_bonus_ore(void)
{
    return collected * NUG_ORE_EACH + (all_found() ? NUG_ALL_BONUS : 0);
}

const PuzzleOps ops_nugget = {
    .family = FAM_NUGGET,
    .name_str = STR_ROOM_NUGGET,
    .help_str = STR_HELP_NUGGET,
    .key_a_str = STR_KEY_A_BREAK,
    .key_b_str = STR_KEY_B_MARK,
    .load = nugget_load,
    .size = nugget_size,
    .cell = nugget_cell,
    .action = nugget_action,
    .solved = nugget_solved,
    .hint = nugget_hint,
    .save = nugget_save,
    .restore = nugget_restore,
    .bonus_ore = nugget_bonus_ore,
    .immediate_mistakes = true,
};
