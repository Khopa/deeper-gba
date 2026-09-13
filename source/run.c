#include "run.h"
#include "rng.h"
#include "bank.h"
#include "save.h"

static bool family_ok[FAM_COUNT];

void run_set_available_families(const bool available[FAM_COUNT])
{
    for (int f = 0; f < FAM_COUNT; f++) family_ok[f] = available[f];
}

// --- difficulty curve -------------------------------------------------------------

int run_base_difficulty(int layer)
{
    static const int saw[4] = { 0, 1, 0, -1 };
    int base = 1 + layer * 8 / (RUN_LAYERS - 1);          // 1..9
    int d = base + saw[layer & 3];
    if (layer < 3 && d > 2) d = 2;                         // gentle start, every run
    return clampi(d, DIFF_MIN, DIFF_MAX);
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
    u8 x[3][RUN_LAYERS];
    for (int p = 0; p < 3; p++) {
        x[p][0] = 1;
        x[p][RUN_LAYERS - 1] = 1;
        for (int l = 1; l < RUN_LAYERS - 1; l++) {
            int lo = x[p][l - 1] > 0 ? x[p][l - 1] - 1 : 0;
            int hi = x[p][l - 1] < RUN_SLOTS - 1 ? x[p][l - 1] + 1 : RUN_SLOTS - 1;
            if (p > 0 && lo < x[p - 1][l]) lo = x[p - 1][l];   // keep paths ordered: no crossings
            if (lo > hi) lo = hi;
            x[p][l] = (u8)(lo + (int)rng_range((u32)(hi - lo + 1)));
        }
    }
    // paths must be able to converge on the core: layer RUN_LAYERS-2 -> slot 1 is always adjacent
    for (int p = 0; p < 3; p++)
        for (int l = 0; l < RUN_LAYERS; l++) {
            rs->node[l][x[p][l]].present = 1;
            if (l + 1 < RUN_LAYERS) rs->edges[l] |= (u16)(1 << (x[p][l] * RUN_SLOTS + x[p][l + 1]));
        }
}

void run_new(RunState *rs, u32 seed, const u16 *recent, int n_recent)
{
    memset(rs, 0, sizeof *rs);
    rs->seed = seed;
    rng_seed(seed);
    rs->lives = rs->max_lives = 3;
    rs->hints = 3;
    memset(rs->path, RUN_NO_SLOT, sizeof rs->path);

    build_paths(rs);

    bool nuggets = family_ok[FAM_NUGGET];
    for (int l = 0; l < RUN_LAYERS; l++) {
        int camp_slot = (l == 9 || l == 19) ? -1 : -2;   // -1: a camp still to place in this layer
        for (int s = 0; s < RUN_SLOTS; s++) {
            RunNode *n = &rs->node[l][s];
            if (!n->present) continue;
            n->difficulty = (u8)run_base_difficulty(l);
            if (l == RUN_LAYERS - 1) {
                n->kind = NODE_CORE;
                n->family = FAM_DIG;
                n->difficulty = DIFF_MAX;
            } else if (l == 0) {
                n->kind = NODE_PUZZLE;
                n->family = pick_family(rs, l, false);
            } else {
                int roll = (int)rng_range(100);
                if (camp_slot == -1 && roll < 40) { n->kind = NODE_CAMP; camp_slot = s; }
                else if (roll < 12 && l >= 4)      n->kind = NODE_RISKY;
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
        if (camp_slot == -1)                                // nobody rolled the camp: force one
            for (int s = 0; s < RUN_SLOTS; s++)
                if (rs->node[l][s].present) { rs->node[l][s].kind = NODE_CAMP; break; }
    }
    rs->layer = 0;
    rs->slot = 1;
    rs->path[0] = 1;
}

// --- traversal ---------------------------------------------------------------------------

int run_next_choices(const RunState *rs)
{
    if (rs->layer >= RUN_LAYERS - 1) return 0;
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
bool run_at_core(const RunState *rs) { return rs->layer == RUN_LAYERS - 1; }

void run_room_cleared(RunState *rs, int ore_gained)
{
    const RunNode *n = run_current(rs);
    rs->ore = (u16)clampi(rs->ore + ore_gained, 0, 9999);
    if (n->kind == NODE_HINT) rs->hints = (u8)clampi(rs->hints + 1, 0, 9);
    if (n->kind == NODE_LIFE || n->kind == NODE_CAMP) rs->lives = (u8)clampi(rs->lives + 1, 0, RUN_MAX_LIVES);
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
    if (powers & (1u << POWER_TOUGH)) { rs->lives++; rs->max_lives++; }
    rs->second_chance = (powers & (1u << POWER_SECOND_CHANCE)) ? 1 : 0;
}

bool run_is_over(const RunState *rs) { return rs->lives == 0; }
