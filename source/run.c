#include "run.h"
#include "heart.h"
#include "rng.h"
#include "bank.h"
#include "save.h"

static bool family_ok[FAM_COUNT];

void run_set_available_families(const bool available[FAM_COUNT])
{
    for (int f = 0; f < FAM_COUNT; f++) family_ok[f] = available[f];
}

// --- difficulty curve -------------------------------------------------------------

int run_length(int index)
{
    static const int lengths[RUN_LENGTHS] = { 15, 30, 60 };
    return lengths[clampi(index, 0, RUN_LENGTHS - 1)];
}

int run_base_difficulty(int layer, int layers)
{
    static const int saw[4] = { 0, 1, 0, -1 };
    int base = 1 + layer * 8 / (layers - 1);               // 1..9
    int d = base + saw[layer & 3];
    if (layer < 3 && d > 2) d = 2;                         // gentle start, every run
    return clampi(d, DIFF_MIN, DIFF_MAX);
}

int run_time_budget(int difficulty)
{
    return (45 + 15 * difficulty) * 30;                    // 30 s at difficulty 1, 97 s at 10; the lantern adds up to 150%
}

int run_stability(const RunNode *n)
{
    if (n->family == FAM_NUGGET) return 6;                 // misses allowed while prospecting
    switch (n->kind) {
    case NODE_RISKY: return 2;
    case NODE_PUZZLE: return 4;
    default: return 3;
    }
}

int run_reward_ore(const RunNode *n)
{
    if (n->family == FAM_NUGGET) return 0;                 // paid per nugget found
    int ore = 10 + n->difficulty * 5;
    if (n->kind == NODE_RISKY) ore *= 2;
    if (n->kind == NODE_CORE) ore *= 3;
    return ore;
}

// --- map building ------------------------------------------------------------------

static bool is_recent(const u16 *recent, int n_recent, int family, int index)
{
    u16 key = (u16)((family << 12) | index);
    for (int i = 0; i < n_recent; i++) if (recent[i] == key) return true;
    return false;
}

static int pick_puzzle(int family, int difficulty, const u16 *recent, int n_recent)
{
    int first = 0, last = 0;
    // widen the window step by step so a family without very hard puzzles
    // still serves its hardest ones deep down
    for (int spread = 1; spread <= DIFF_MAX && last <= first; spread++)
        bank_range(family, difficulty - spread, difficulty + spread, &first, &last);
    if (last <= first) return 0;
    int pick = first;
    for (int attempt = 0; attempt < 8; attempt++) {
        pick = first + (int)rng_range((u32)(last - first));
        if (!is_recent(recent, n_recent, family, pick)) break;
    }
    return pick;
}

static int pick_family(const RunState *rs, int layer, bool allow_nugget)
{
    int cands[FAM_COUNT], n = 0;
    for (int f = 0; f < FAM_COUNT; f++) {
        if (!family_ok[f]) continue;
        if (f == FAM_NUGGET && !allow_nugget) continue;
        if (f == FAM_HEART) continue;                      // the core only
        cands[n++] = f;
        if (f == FAM_DIG) cands[n++] = f;                  // the signature puzzle is twice as likely
    }
    if (!n) return FAM_DIG;
    // prefer a family absent from the previous layer
    for (int attempt = 0; attempt < 4; attempt++) {
        int f = cands[rng_range((u32)n)];
        bool seen = false;
        if (layer > 0)
            for (int s = 0; s < RUN_SLOTS; s++)
                if (rs->node[layer - 1][s].present && rs->node[layer - 1][s].family == f) seen = true;
        if (!seen || attempt == 3) return f;
    }
    return cands[0];
}

static void build_paths(RunState *rs)
{
    int layers = rs->layers;
    u8 x[3][RUN_MAX_LAYERS];
    for (int p = 0; p < 3; p++) {
        x[p][0] = 1;
        x[p][layers - 1] = 1;
        for (int l = 1; l < layers - 1; l++) {
            int lo = x[p][l - 1] > 0 ? x[p][l - 1] - 1 : 0;
            int hi = x[p][l - 1] < RUN_SLOTS - 1 ? x[p][l - 1] + 1 : RUN_SLOTS - 1;
            if (p > 0 && lo < x[p - 1][l]) lo = x[p - 1][l];   // keep paths ordered: no crossings
            if (lo > hi) lo = hi;
            x[p][l] = (u8)(lo + (int)rng_range((u32)(hi - lo + 1)));
        }
    }
    // paths must be able to converge on the core: layer layers-2 -> slot 1 is always adjacent
    for (int p = 0; p < 3; p++)
        for (int l = 0; l < layers; l++) {
            rs->node[l][x[p][l]].present = 1;
            if (l + 1 < layers) rs->edges[l] |= (u16)(1 << (x[p][l] * RUN_SLOTS + x[p][l + 1]));
        }
}

void run_new(RunState *rs, u32 seed, int length_index, const u16 *recent, int n_recent)
{
    memset(rs, 0, sizeof *rs);
    rs->seed = seed;
    rng_seed(seed);
    rs->lives = rs->max_lives = 1;                     // the counter sells more (max lives, then starting lives)
    rs->hints = 3;
    rs->length_index = (u8)clampi(length_index, 0, RUN_LENGTHS - 1);
    rs->layers = (u8)run_length(rs->length_index);
    memset(rs->path, RUN_NO_SLOT, sizeof rs->path);

    build_paths(rs);

    int layers = rs->layers;
    // A rest stop at each third of the descent: the whole layer is camps, so
    // every path gets its two rests (and two visits to the merchant).
    int camp_a = layers / 3, camp_b = 2 * layers / 3;
    bool nuggets = family_ok[FAM_NUGGET];
    for (int l = 0; l < layers; l++) {
        for (int s = 0; s < RUN_SLOTS; s++) {
            RunNode *n = &rs->node[l][s];
            if (!n->present) continue;
            n->difficulty = (u8)run_base_difficulty(l, layers);
            if (l == camp_a || l == camp_b) {
                n->kind = NODE_CAMP;
                n->family = FAM_DIG;
            } else if (l == layers - 1) {
                n->kind = NODE_CORE;
                if (family_ok[FAM_HEART]) {           // the picture grows with the descent
                    n->family = FAM_HEART;
                    n->difficulty = (u8)heart_difficulty(heart_sizes[rs->length_index]);
                } else {
                    n->family = FAM_DIG;
                    n->difficulty = DIFF_MAX;
                }
            } else if (l == 0) {
                n->kind = NODE_PUZZLE;          // the entrance is always the signature puzzle
                n->family = family_ok[FAM_DIG] ? FAM_DIG : pick_family(rs, l, false);
            } else {
                int roll = (int)rng_range(100);
                if (roll < 12 && l >= 4)           n->kind = NODE_RISKY;
                else if (roll < 24)                n->kind = NODE_HINT;
                else if (roll < 29)                n->kind = NODE_LIFE;
                else                               n->kind = NODE_PUZZLE;
                bool nugget_here = nuggets && l >= 2 && n->kind == NODE_PUZZLE && rng_range(100) < 12;
                n->family = nugget_here ? FAM_NUGGET : pick_family(rs, l, false);
                if (n->kind == NODE_RISKY) n->difficulty = (u8)clampi(n->difficulty + 2, DIFF_MIN, DIFF_MAX);
            }
            if (n->kind != NODE_CAMP && n->family != FAM_NUGGET)
                n->puzzle = (u16)pick_puzzle(n->family, n->difficulty, recent, n_recent);
        }
    }
    rs->layer = 0;
    rs->slot = 1;
    rs->path[0] = 1;
}

// --- traversal ---------------------------------------------------------------------------

int run_next_choices(const RunState *rs)
{
    if (rs->layer >= rs->layers - 1) return 0;
    int mask = 0;
    for (int to = 0; to < RUN_SLOTS; to++)
        if (rs->edges[rs->layer] & (1 << (rs->slot * RUN_SLOTS + to))) mask |= 1 << to;
    return mask;
}

bool run_can_go(const RunState *rs, int slot) { return (run_next_choices(rs) >> slot) & 1; }

void run_go(RunState *rs, int slot)
{
    if (!run_can_go(rs, slot)) return;
    rs->layer++;
    rs->slot = (u8)slot;
    rs->path[rs->layer] = (u8)slot;
    rs->room_in_progress = 0;
}

const RunNode *run_current(const RunState *rs) { return &rs->node[rs->layer][rs->slot]; }
bool run_at_core(const RunState *rs) { return rs->layer == rs->layers - 1; }

void run_room_cleared(RunState *rs, int ore_gained)
{
    const RunNode *n = run_current(rs);
    rs->ore = (u16)clampi(rs->ore + ore_gained, 0, 9999);
    if (n->kind == NODE_HINT) rs->hints = (u8)clampi(rs->hints + 1, 0, 9);
    if (n->kind == NODE_LIFE || n->kind == NODE_CAMP) rs->lives = (u8)clampi(rs->lives + 1, 0, rs->max_lives);
    rs->room_in_progress = 0;
}

void run_room_failed(RunState *rs)
{
    if (rs->second_chance) rs->second_chance = 0;
    else if (rs->lives) rs->lives--;
    rs->room_in_progress = 0;
}

void run_apply_powers(RunState *rs, u32 powers)
{
    if (powers & (1u << POWER_LAMP))  rs->hints = (u8)clampi(rs->hints + 1, 0, 9);
    if (powers & (1u << POWER_TOUGH)) { rs->max_lives = (u8)clampi(rs->max_lives + 1, 1, RUN_MAX_LIVES); rs->lives = (u8)clampi(rs->lives + 1, 1, rs->max_lives); }
    rs->second_chance = (powers & (1u << POWER_SECOND_CHANCE)) ? 1 : 0;
}

bool run_is_over(const RunState *rs) { return rs->lives == 0; }

void run_reroll_core(RunState *rs)
{
    RunNode *n = &rs->node[rs->layer][rs->slot];
    if (n->kind != NODE_CORE || bank_count(n->family) < 2) return;
    u16 was = n->puzzle;
    for (int attempt = 0; attempt < 8 && n->puzzle == was; attempt++)
        n->puzzle = (u16)pick_puzzle(n->family, n->difficulty, NULL, 0);
}
