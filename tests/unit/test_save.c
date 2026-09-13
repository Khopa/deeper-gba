// SRAM save blocks (source/save.c) on the host shim's fake cartridge.
#include "test.h"
#include "save.h"
#include "host_shim.h"

TEST(blank_cartridge_gives_defaults_and_no_run)
{
    save_init();
    Profile *p = save_profile();
    CHECK_EQ(p->runs_started, 0);
    CHECK_EQ(p->powers, 0);
    CHECK_EQ(p->sound, 1);
    CHECK_EQ(p->lengths_unlocked, 1);
    CHECK_EQ(p->best_frames[0] + p->best_frames[1] + p->best_frames[2], 0);
    CHECK_EQ(p->last_frames, 0);
    CHECK(!save_has_run());
    CHECK_EQ(REG_WAITCNT & 3, WS_SRAM_8);
    // the defaults were written back: a second init reads a valid block
    host_io[0x204 / 2] = 0;
    save_init();
    CHECK_EQ(save_profile()->sound, 1);
}

TEST(profile_round_trip_and_corruption)
{
    save_init();
    Profile *p = save_profile();
    p->runs_started = 7;
    p->best_depth = 23;
    p->total_ore = 12345;
    p->powers = 5;
    p->lengths_unlocked = 2;
    p->best_frames[0] = 54321;
    p->last_frames = 999;
    p->total_frames = 123456;
    save_profile_add_recent(FAM_DIG, 300);
    save_profile_commit();

    save_init();                                 // re-read from SRAM
    p = save_profile();
    CHECK_EQ(p->lengths_unlocked, 2);
    CHECK_EQ(p->best_frames[0], 54321);
    CHECK_EQ(p->last_frames, 999);
    CHECK_EQ(p->total_frames, 123456);
    CHECK_EQ(p->runs_started, 7);
    CHECK_EQ(p->best_depth, 23);
    CHECK_EQ(p->total_ore, 12345);
    CHECK_EQ(p->powers, 5);
    CHECK_EQ(p->recent[0], (FAM_DIG << 12) | 300);
    CHECK_EQ(p->recent_head, 1);

    host_sram[SAVE_PROFILE_OFF + 20] ^= 0xFF;    // flip a payload byte
    save_init();
    CHECK_EQ(save_profile()->runs_started, 0);   // back to defaults
}

TEST(recent_ring_wraps)
{
    save_init();
    for (int i = 0; i < RECENT_MAX + 3; i++) save_profile_add_recent(FAM_DIG, i);
    Profile *p = save_profile();
    CHECK_EQ(p->recent_head, 3);
    CHECK_EQ(p->recent[0], (FAM_DIG << 12) | RECENT_MAX);
    CHECK_EQ(p->recent[3], (FAM_DIG << 12) | 3);
}

TEST(run_block_round_trip_with_and_without_room)
{
    save_init();
    RunState rs;
    memset(&rs, 0, sizeof rs);
    rs.seed = 0xC0FFEE;
    rs.layers = 60;
    rs.frames = 100000;
    rs.layer = 12;
    rs.slot = 2;
    rs.lives = 2;
    rs.ore = 321;
    rs.node[12][2].present = 1;
    rs.node[12][2].puzzle = 77;
    save_run_commit(&rs, NULL);
    CHECK(save_has_run());

    RunState back;
    RoomSave room;
    memset(&room, 0xAA, sizeof room);
    CHECK(save_run_load(&back, &room));
    CHECK_EQ(back.seed, 0xC0FFEE);
    CHECK_EQ(back.layers, 60);
    CHECK_EQ(back.frames, 100000);
    CHECK_EQ(back.layer, 12);
    CHECK_EQ(back.ore, 321);
    CHECK_EQ(back.node[12][2].puzzle, 77);
    CHECK_EQ(back.room_in_progress, 0);
    CHECK_EQ(room.len, 0);

    RoomSave rsave = { .len = 5, .stability = 3, .hints_left = 2, .mistakes = 1, .hints_used = 1, .cur_r = 4, .cur_c = 3, .elapsed = 4321 };
    memcpy(rsave.data, "hello", 5);
    save_run_commit(&rs, &rsave);
    CHECK(save_run_load(&back, &room));
    CHECK_EQ(back.room_in_progress, 1);
    CHECK_EQ(room.len, 5);
    CHECK_EQ(room.stability, 3);
    CHECK_EQ(room.cur_c, 3);
    CHECK_EQ(room.elapsed, 4321);
    CHECK_MEM(room.data, "hello", 5);

    // survives a reboot, dies with a clear
    save_init();
    CHECK(save_has_run());
    save_run_clear();
    CHECK(!save_has_run());
    save_init();
    CHECK(!save_has_run());
    CHECK(!save_run_load(&back, &room));
}

TEST(run_block_rejects_corruption_and_keeps_profile)
{
    save_init();
    save_profile()->runs_won = 2;
    save_profile_commit();
    RunState rs;
    memset(&rs, 0, sizeof rs);
    rs.lives = 3;
    save_run_commit(&rs, NULL);
    host_sram[SAVE_RUN_OFF + 40] ^= 0x01;
    save_init();
    CHECK(!save_has_run());
    CHECK_EQ(save_profile()->runs_won, 2);
}

TEST(unlocks_follow_milestones_once)
{
    save_init();
    Profile *p = save_profile();
    CHECK_EQ(save_check_unlocks(), 0);
    p->best_depth = 10;
    CHECK_EQ(save_check_unlocks(), 1u << POWER_LAMP);
    CHECK_EQ(save_check_unlocks(), 0);           // already owned
    p->best_depth = 25;
    p->runs_won = 1;
    u32 fresh = save_check_unlocks();
    CHECK_EQ(fresh, (1u << POWER_TOUGH) | (1u << POWER_SECOND_CHANCE));
    CHECK_EQ(p->powers, (1u << POWER_LAMP) | (1u << POWER_TOUGH) | (1u << POWER_SECOND_CHANCE));
}

const TestCase save_tests[] = {
    T(blank_cartridge_gives_defaults_and_no_run),
    T(profile_round_trip_and_corruption),
    T(recent_ring_wraps),
    T(run_block_round_trip_with_and_without_room),
    T(run_block_rejects_corruption_and_keeps_profile),
    T(unlocks_follow_milestones_once),
};
SUITE(save)
