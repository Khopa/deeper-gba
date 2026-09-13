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
    run_new(&a, 1234, NULL, 0);
    run_new(&b, 1234, NULL, 0);
    run_new(&c, 1235, NULL, 0);
    CHECK_MEM(&a, &b, sizeof a);
    CHECK(memcmp(&a, &c, sizeof a) != 0);
}

TEST(map_shape_surface_core_and_connectivity)
{
    setup();
    for (u32 seed = 1; seed <= 40; seed++) {
        RunState rs;
        run_new(&rs, seed, NULL, 0);
        CHECK_EQ(present_count(&rs, 0), 1);
        CHECK(rs.node[0][1].present);
        CHECK_EQ(rs.node[0][1].family, FAM_DIG);          // the entrance is always a DIG room
        CHECK_EQ(present_count(&rs, RUN_LAYERS - 1), 1);
        CHECK_EQ(rs.node[RUN_LAYERS - 1][1].kind, NODE_CORE);
        for (int l = 0; l < RUN_LAYERS; l++) {
            CHECK(present_count(&rs, l) >= 1);
            for (int s = 0; s < RUN_SLOTS; s++) {
                if (!rs.node[l][s].present) continue;
                if (l + 1 < RUN_LAYERS) {           // at least one way down, into present nodes only
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
    run_new(&rs, 77, NULL, 0);
    for (int l = 0; l < RUN_LAYERS; l++)
        if (present_count(&rs, l) >= 2) layers_with_choice++;
    CHECK(layers_with_choice >= 10);
    for (int seed = 1; seed <= 20; seed++) {
        run_new(&rs, (u32)seed, NULL, 0);
        int camps9 = 0, camps19 = 0, camps_other = 0;
        for (int l = 0; l < RUN_LAYERS; l++)
            for (int s = 0; s < RUN_SLOTS; s++)
                if (rs.node[l][s].present && rs.node[l][s].kind == NODE_CAMP) {
                    if (l == 9) camps9++; else if (l == 19) camps19++; else camps_other++;
                }
        CHECK_EQ(camps9, 1);
        CHECK_EQ(camps19, 1);
        CHECK_EQ(camps_other, 0);
    }
}

TEST(difficulty_curve_is_gentle_first_then_rises_in_sawtooth)
{
    for (int l = 0; l < 3; l++) CHECK(run_base_difficulty(l) <= 2);
    int early = 0, late = 0;
    for (int l = 3; l < 8; l++) early += run_base_difficulty(l);
    for (int l = 24; l < 29; l++) late += run_base_difficulty(l);
    CHECK(late > early + 10);
    bool dips = false;
    for (int l = 4; l < RUN_LAYERS - 1; l++)
        if (run_base_difficulty(l) < run_base_difficulty(l - 1)) dips = true;
    CHECK(dips);                                   // the sawtooth really goes down sometimes
    for (int l = 0; l < RUN_LAYERS; l++) {
        int d = run_base_difficulty(l);
        CHECK(d >= DIFF_MIN && d <= DIFF_MAX);
    }
}

TEST(nodes_carry_valid_puzzles_near_their_difficulty)
{
    setup();
    RunState rs;
    run_new(&rs, 4242, NULL, 0);
    for (int l = 0; l < RUN_LAYERS; l++)
        for (int s = 0; s < RUN_SLOTS; s++) {
            const RunNode *n = &rs.node[l][s];
            if (!n->present || n->kind == NODE_CAMP) continue;
            CHECK_EQ(n->family, FAM_DIG);
            CHECK(n->difficulty >= DIFF_MIN && n->difficulty <= DIFF_MAX);
            BankEntry e;
            CHECK(bank_get(FAM_DIG, n->puzzle, &e));
            int diff = e.hdr->difficulty;
            CHECK(abs(diff - n->difficulty) <= 1);
            if (n->kind == NODE_RISKY) CHECK(l >= 4);
        }
    CHECK(rs.node[RUN_LAYERS - 1][1].difficulty == DIFF_MAX);
}

TEST(recent_puzzles_are_avoided_when_possible)
{
    setup();
    RunState a;
    run_new(&a, 99, NULL, 0);
    // ban every puzzle of run a, rebuild with the same seed: the picks must move
    u16 recent[RUN_LAYERS * RUN_SLOTS];
    int n = 0;
    for (int l = 0; l < RUN_LAYERS; l++)
        for (int s = 0; s < RUN_SLOTS; s++)
            if (a.node[l][s].present && a.node[l][s].kind != NODE_CAMP)
                recent[n++] = (u16)((FAM_DIG << 12) | a.node[l][s].puzzle);
    RunState b;
    run_new(&b, 99, recent, n);
    int moved = 0, total = 0;
    for (int l = 0; l < RUN_LAYERS; l++)
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
    run_new(&rs, 5, NULL, 0);
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
    run_new(&rs, 8, NULL, 0);
    CHECK_EQ(rs.lives, 3);
    CHECK_EQ(rs.hints, 3);
    CHECK_EQ(rs.ore, 0);
    run_room_cleared(&rs, 25);
    CHECK_EQ(rs.ore, 25);
    run_room_failed(&rs);
    CHECK_EQ(rs.lives, 2);
    CHECK(!run_is_over(&rs));
    run_room_failed(&rs);
    run_room_failed(&rs);
    CHECK(run_is_over(&rs));
    run_room_failed(&rs);
    CHECK_EQ(rs.lives, 0);

    // a hint node grants a hint, a camp a life (capped)
    rs.node[rs.layer][rs.slot].kind = NODE_HINT;
    run_room_cleared(&rs, 0);
    CHECK_EQ(rs.hints, 4);
    rs.node[rs.layer][rs.slot].kind = NODE_CAMP;
    rs.lives = RUN_MAX_LIVES;
    run_room_cleared(&rs, 0);
    CHECK_EQ(rs.lives, RUN_MAX_LIVES);
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

TEST(powers_change_starting_resources_and_second_chance)
{
    setup();
    RunState rs;
    run_new(&rs, 3, NULL, 0);
    run_apply_powers(&rs, 0);
    CHECK_EQ(rs.hints, 3);
    CHECK_EQ(rs.lives, 3);
    CHECK_EQ(rs.second_chance, 0);
    run_new(&rs, 3, NULL, 0);
    run_apply_powers(&rs, (1u << POWER_LAMP) | (1u << POWER_TOUGH) | (1u << POWER_SECOND_CHANCE));
    CHECK_EQ(rs.hints, 4);
    CHECK_EQ(rs.lives, 4);
    CHECK_EQ(rs.max_lives, 4);
    CHECK_EQ(rs.second_chance, 1);
    run_room_failed(&rs);                          // the free one
    CHECK_EQ(rs.lives, 4);
    CHECK_EQ(rs.second_chance, 0);
    run_room_failed(&rs);
    CHECK_EQ(rs.lives, 3);
}

const TestCase run_tests[] = {
    T(powers_change_starting_resources_and_second_chance),
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
