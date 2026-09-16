// Run map generation and traversal (source/run.c) on the committed DIG bank.
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "run.h"
#include "bank.h"
#include "save.h"

static uint8_t *img;
static int img_len;

static void setup(void)
{
    if (!img) {
        FILE *f = fopen("data/puzzles/dig.bin", "rb");
        CHECK(f != NULL);
        if (!f) return;
        fseek(f, 0, SEEK_END);
        img_len = (int)ftell(f);
        fseek(f, 0, SEEK_SET);
        img = malloc(img_len);
        CHECK(fread(img, 1, img_len, f) == (size_t)img_len);
        fclose(f);
    }
    bank_init();
    bank_register(img, img_len);
    bool avail[FAM_COUNT] = { [FAM_DIG] = true };
    run_set_available_families(avail);
}

static int present_count(const RunState *rs, int l)
{
    int n = 0;
    for (int s = 0; s < RUN_SLOTS; s++) n += rs->node[l][s].present;
    return n;
}

TEST(map_is_deterministic_per_seed)
{
    setup();
    RunState a, b, c;
    run_new(&a, 1234, 1, NULL, 0);
    run_new(&b, 1234, 1, NULL, 0);
    run_new(&c, 1235, 1, NULL, 0);
    CHECK_MEM(&a, &b, sizeof a);
    CHECK(memcmp(&a, &c, sizeof a) != 0);
}

TEST(map_shape_surface_core_and_connectivity)
{
    setup();
    for (u32 seed = 1; seed <= 40; seed++) {
        RunState rs;
        run_new(&rs, seed, 1, NULL, 0);
        CHECK_EQ(present_count(&rs, 0), 1);
        CHECK(rs.node[0][1].present);
        CHECK_EQ(rs.node[0][1].family, FAM_DIG);          // the entrance is always a DIG room
        CHECK_EQ(present_count(&rs, rs.layers - 1), 1);
        CHECK_EQ(rs.node[rs.layers - 1][1].kind, NODE_CORE);
        for (int l = 0; l < rs.layers; l++) {
            CHECK(present_count(&rs, l) >= 1);
            for (int s = 0; s < RUN_SLOTS; s++) {
                if (!rs.node[l][s].present) continue;
                if (l + 1 < rs.layers) {           // at least one way down, into present nodes only
                    int out = 0;
                    for (int t = 0; t < RUN_SLOTS; t++)
                        if (rs.edges[l] & (1 << (s * RUN_SLOTS + t))) { out++; CHECK(rs.node[l + 1][t].present); }
                    CHECK(out >= 1);
                }
                if (l > 0) {                        // and at least one way in
                    int in = 0;
                    for (int f = 0; f < RUN_SLOTS; f++)
                        if (rs.edges[l - 1] & (1 << (f * RUN_SLOTS + s))) in++;
                    CHECK(in >= 1);
                }
            }
            // no crossing edges: a < c implies b <= d
            for (int a = 0; a < RUN_SLOTS; a++)
                for (int b = 0; b < RUN_SLOTS; b++) {
                    if (!(rs.edges[l] & (1 << (a * RUN_SLOTS + b)))) continue;
                    for (int c = a + 1; c < RUN_SLOTS; c++)
                        for (int d = 0; d < RUN_SLOTS; d++)
                            if (rs.edges[l] & (1 << (c * RUN_SLOTS + d))) CHECK(b <= d);
                }
        }
    }
}

TEST(map_has_branches_and_camps)
{
    setup();
    int layers_with_choice = 0;
    RunState rs;
    run_new(&rs, 77, 1, NULL, 0);
    for (int l = 0; l < rs.layers; l++)
        if (present_count(&rs, l) >= 2) layers_with_choice++;
    CHECK(layers_with_choice >= 10);
    for (int seed = 1; seed <= 20; seed++) {
        run_new(&rs, (u32)seed, 1, NULL, 0);
        int camps9 = 0, camps19 = 0, camps_other = 0;
        for (int l = 0; l < rs.layers; l++)
            for (int s = 0; s < RUN_SLOTS; s++)
                if (rs.node[l][s].present && rs.node[l][s].kind == NODE_CAMP) {
                    if (l == 10) camps9++; else if (l == 20) camps19++; else camps_other++;
                }
        CHECK_EQ(camps9, present_count(&rs, 10));   // a third of the way down: the whole layer rests
        CHECK_EQ(camps19, present_count(&rs, 20));  // two thirds
        CHECK_EQ(camps_other, 0);
    }
}

TEST(difficulty_curve_is_gentle_first_then_rises_in_sawtooth)
{
    for (int li = 0; li < RUN_LENGTHS; li++) {
        int layers = run_length(li);
        for (int l = 0; l < 3; l++) CHECK(run_base_difficulty(l, layers) <= 2);
        int early = 0, late = 0;
        for (int l = 3; l < 8; l++) early += run_base_difficulty(l, layers);
        for (int l = layers - 6; l < layers - 1; l++) late += run_base_difficulty(l, layers);
        CHECK(late > early + 10);
        bool dips = false;
        for (int l = 4; l < layers - 1; l++)
            if (run_base_difficulty(l, layers) < run_base_difficulty(l - 1, layers)) dips = true;
        CHECK(dips);                               // the sawtooth really goes down sometimes
        for (int l = 0; l < layers; l++) {
            int d = run_base_difficulty(l, layers);
            CHECK(d >= DIFF_MIN && d <= DIFF_MAX);
        }
    }
    CHECK_EQ(run_length(0), 15);
    CHECK_EQ(run_length(1), 30);
    CHECK_EQ(run_length(2), 60);
    CHECK(run_time_budget(1) < run_time_budget(10));
    CHECK_EQ(run_time_budget(1), 30 * 60);
}

TEST(nodes_carry_valid_puzzles_near_their_difficulty)
{
    setup();
    RunState rs;
    run_new(&rs, 4242, 1, NULL, 0);
    for (int l = 0; l < rs.layers; l++)
        for (int s = 0; s < RUN_SLOTS; s++) {
            const RunNode *n = &rs.node[l][s];
            if (!n->present || n->kind == NODE_CAMP || n->kind == NODE_CRATES || n->kind == NODE_FIGHT || n->kind == NODE_WALL) continue;
            CHECK_EQ(n->family, FAM_DIG);
            CHECK(n->difficulty >= DIFF_MIN && n->difficulty <= DIFF_MAX);
            BankEntry e;
            CHECK(bank_get(FAM_DIG, n->puzzle, &e));
            int diff = e.hdr->difficulty;
            CHECK(abs(diff - n->difficulty) <= 1);
            if (n->kind == NODE_RISKY) CHECK(l >= 4);
        }
    CHECK(rs.node[rs.layers - 1][1].difficulty == DIFF_MAX);
}

TEST(recent_puzzles_are_avoided_when_possible)
{
    setup();
    RunState a;
    run_new(&a, 99, 1, NULL, 0);
    // ban every puzzle of run a, rebuild with the same seed: the picks must move
    u16 recent[RUN_MAX_LAYERS * RUN_SLOTS];
    int n = 0;
    for (int l = 0; l < a.layers; l++)
        for (int s = 0; s < RUN_SLOTS; s++)
            if (a.node[l][s].present && a.node[l][s].kind != NODE_CAMP)
                recent[n++] = (u16)((FAM_DIG << 12) | a.node[l][s].puzzle);
    RunState b;
    run_new(&b, 99, 1, recent, n);
    int moved = 0, total = 0;
    for (int l = 0; l < b.layers; l++)
        for (int s = 0; s < RUN_SLOTS; s++)
            if (b.node[l][s].present && b.node[l][s].kind != NODE_CAMP) {
                total++;
                bool banned = false;
                for (int i = 0; i < n; i++) if (recent[i] == (u16)((FAM_DIG << 12) | b.node[l][s].puzzle)) banned = true;
                if (!banned) moved++;
            }
    CHECK(moved * 4 >= total * 3);                 // most nodes found an unplayed puzzle
}

TEST(traversal_follows_edges_to_the_core)
{
    setup();
    RunState rs;
    run_new(&rs, 5, 1, NULL, 0);
    CHECK_EQ(rs.layer, 0);
    CHECK_EQ(rs.slot, 1);
    CHECK_EQ(rs.path[0], 1);
    int steps = 0;
    while (!run_at_core(&rs) && steps < 100) {
        int choices = run_next_choices(&rs);
        CHECK(choices != 0);
        int slot = 0;
        while (!(choices & (1 << slot))) slot++;
        int before = rs.layer;
        for (int bad = 0; bad < RUN_SLOTS; bad++)  // illegal slots are ignored...
            if (!(choices & (1 << bad))) { run_go(&rs, bad); CHECK_EQ(rs.layer, before); }
        run_go(&rs, slot);                         // ...a legal one moves down
        CHECK_EQ(rs.layer, before + 1);
        CHECK_EQ(rs.slot, slot);
        CHECK_EQ(rs.path[rs.layer], slot);
        steps++;
    }
    CHECK(run_at_core(&rs));
    CHECK_EQ(run_next_choices(&rs), 0);
    CHECK_EQ(run_current(&rs)->kind, NODE_CORE);
}

TEST(resources_follow_room_outcomes)
{
    setup();
    RunState rs;
    run_new(&rs, 8, 1, NULL, 0);
    CHECK_EQ(rs.lives, 1);                         // one life, one maximum: the counter sells more
    CHECK_EQ(rs.max_lives, 1);
    CHECK_EQ(rs.hints, 1);                         // one hint: the merchant's keys add more
    CHECK_EQ(rs.ore, 0);
    run_room_cleared(&rs, 25);
    CHECK_EQ(rs.ore, 25);
    rs.lives = rs.max_lives = 3;
    run_room_failed(&rs, false);
    CHECK_EQ(rs.lives, 2);
    CHECK(!run_is_over(&rs));
    run_room_failed(&rs, true);                    // the helmet took it
    CHECK_EQ(rs.lives, 2);
    CHECK_EQ(rs.room_in_progress, 0);
    run_room_failed(&rs, false);
    run_room_failed(&rs, false);
    CHECK(run_is_over(&rs));
    run_room_failed(&rs, false);
    CHECK_EQ(rs.lives, 0);

    // a hint node grants a hint, a camp a life (capped at the run's maximum)
    rs.node[rs.layer][rs.slot].kind = NODE_HINT;
    run_room_cleared(&rs, 0);
    CHECK_EQ(rs.hints, 2);
    rs.node[rs.layer][rs.slot].kind = NODE_CAMP;
    rs.lives = 1;
    run_room_cleared(&rs, 0);
    CHECK_EQ(rs.lives, 2);
    run_room_cleared(&rs, 0);
    run_room_cleared(&rs, 0);
    CHECK_EQ(rs.lives, 3);                         // max_lives, not RUN_MAX_LIVES
}

TEST(stability_and_reward_depend_on_kind)
{
    RunNode plain = { .present = 1, .kind = NODE_PUZZLE, .difficulty = 4 };
    RunNode risky = { .present = 1, .kind = NODE_RISKY, .difficulty = 4 };
    CHECK(run_stability(&risky) < run_stability(&plain));
    CHECK(run_reward_ore(&risky) > run_reward_ore(&plain));
    RunNode harder = plain;
    harder.difficulty = 8;
    CHECK(run_reward_ore(&harder) > run_reward_ore(&plain));
}

TEST(every_length_builds_a_sound_map)
{
    setup();
    for (int li = 0; li < RUN_LENGTHS; li++) {
        RunState rs;
        run_new(&rs, 777 + (u32)li, li, NULL, 0);
        CHECK_EQ(rs.layers, run_length(li));
        CHECK_EQ(rs.length_index, li);
        CHECK_EQ(rs.frames, 0);
        CHECK(rs.node[0][1].present);
        CHECK_EQ(rs.node[rs.layers - 1][1].kind, NODE_CORE);
        int camps = 0;
        for (int l = 0; l < rs.layers; l++) {
            CHECK(present_count(&rs, l) >= 1);
            for (int s = 0; s < RUN_SLOTS; s++) {
                const RunNode *n = &rs.node[l][s];
                if (!n->present) continue;
                if (n->kind == NODE_CAMP) camps++;
                if (l + 1 < rs.layers) {
                    int out = 0;
                    for (int t = 0; t < RUN_SLOTS; t++)
                        if (rs.edges[l] & (1 << (s * RUN_SLOTS + t))) { out++; CHECK(rs.node[l + 1][t].present); }
                    CHECK(out >= 1);
                }
            }
        }
        for (int l = rs.layers; l < RUN_MAX_LAYERS; l++)
            for (int s = 0; s < RUN_SLOTS; s++) CHECK(!rs.node[l][s].present);
        CHECK_EQ(camps, present_count(&rs, rs.layers / 3) + present_count(&rs, 2 * rs.layers / 3));
        // walk down the leftmost choices: the core is reached in layers-1 steps
        int steps = 0;
        while (!run_at_core(&rs) && steps < 100) {
            int choices = run_next_choices(&rs);
            int slot = 0;
            while (!(choices & (1 << slot))) slot++;
            run_go(&rs, slot);
            steps++;
        }
        CHECK_EQ(steps, rs.layers - 1);
    }
}

static uint8_t *load_bank(const char *path, int *len)
{
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(*len);
    CHECK(fread(data, 1, *len, f) == (size_t)*len);
    fclose(f);
    return data;
}

TEST(families_and_sizes_are_gated_by_layer)
{
    setup();                                       // the dig bank
    static uint8_t *ledger, *block;
    static int ledger_len, block_len;
    if (!ledger) ledger = load_bank("data/puzzles/ledger.bin", &ledger_len);
    if (!block) block = load_bank("data/puzzles/block.bin", &block_len);
    CHECK(bank_register(ledger, ledger_len));
    CHECK(bank_register(block, block_len));
    bool avail[FAM_COUNT] = { [FAM_DIG] = true, [FAM_LEDGER] = true, [FAM_BLOCK] = true, [FAM_NUGGET] = true };
    run_set_available_families(avail);
    int blocks_late = 0, nuggets_late = 0, crates = 0, fights = 0, walls = 0, big_ledgers_late = 0, big_blocks_late = 0;
    for (int seed = 1; seed <= 30; seed++) {
        RunState rs;
        run_new(&rs, (u32)seed, 2, NULL, 0);
        for (int l = 0; l < rs.layers; l++)
            for (int sl = 0; sl < RUN_SLOTS; sl++) {
                const RunNode *n = &rs.node[l][sl];
                if (!n->present || n->kind == NODE_CAMP || n->kind == NODE_CORE) continue;
                if (n->kind == NODE_CRATES) { crates++; CHECK(l >= CRATES_FIRST_LAYER); continue; }
                if (n->kind == NODE_FIGHT) { fights++; CHECK(l >= FIGHT_FIRST_LAYER); CHECK_EQ(run_reward_ore(n), (10 + n->difficulty * 5) * 2); continue; }
                if (n->kind == NODE_WALL) { walls++; CHECK(l >= WALL_FIRST_LAYER); CHECK_EQ(run_reward_ore(n), 10 + n->difficulty * 5); continue; }
                if (n->family == FAM_BLOCK) { CHECK(l >= BLOCK_FIRST_LAYER); blocks_late++; }
                if (n->family == FAM_NUGGET) { CHECK(l >= NUGGET_FIRST_LAYER); nuggets_late++; }
                if (n->family == FAM_LEDGER || n->family == FAM_BLOCK) {
                    BankEntry e;
                    CHECK(bank_get(n->family, n->puzzle, &e));
                    int cap = run_size_cap(n->family, l);
                    if (cap) CHECK(e.hdr->size <= cap);
                    else if (e.hdr->size > 6) { if (n->family == FAM_LEDGER) big_ledgers_late++; else big_blocks_late++; }
                }
            }
    }
    CHECK(blocks_late > 0);
    CHECK(nuggets_late > 0);
    CHECK(crates > 0);
    CHECK(fights > 0);
    CHECK(walls > 0);
    CHECK(big_ledgers_late > 0);                   // past layer 50 the 8x8 ledgers do come
    CHECK(big_blocks_late > 0);
    CHECK_EQ(run_size_cap(FAM_LEDGER, 10), 6);
    CHECK_EQ(run_size_cap(FAM_LEDGER, 55), 0);
    CHECK_EQ(run_size_cap(FAM_DIG, 0), 0);
}

const TestCase run_tests[] = {
    T(families_and_sizes_are_gated_by_layer),
    T(every_length_builds_a_sound_map),
    T(map_is_deterministic_per_seed),
    T(map_shape_surface_core_and_connectivity),
    T(map_has_branches_and_camps),
    T(difficulty_curve_is_gentle_first_then_rises_in_sawtooth),
    T(nodes_carry_valid_puzzles_near_their_difficulty),
    T(recent_puzzles_are_avoided_when_possible),
    T(traversal_follows_edges_to_the_core),
    T(resources_follow_room_outcomes),
    T(stability_and_reward_depend_on_kind),
};
SUITE(run)
