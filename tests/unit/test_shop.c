// The merchant's catalogue and what buying does (source/shop.c).
#include "test.h"
#include "shop.h"

static void fresh(Profile *p, RunState *rs)
{
    memset(p, 0, sizeof *p);
    memset(rs, 0, sizeof *rs);
    rs->lives = rs->max_lives = 3;
    rs->hints = 3;
    rs->ore = 100;
    p->ore_bank = 1000;
}

TEST(catalogue_is_split_between_camp_and_counter)
{
    CHECK_EQ(shop_first(SHOP_CAMP), ITEM_POTION);
    CHECK_EQ(shop_count(SHOP_CAMP), 3);
    CHECK_EQ(shop_first(SHOP_META), ITEM_BEER);
    CHECK_EQ(shop_count(SHOP_META), 5);
    CHECK_EQ(shop_first(SHOP_META) + shop_count(SHOP_META), ITEM_COUNT);
    CHECK_EQ(SHOP_META_COUNT, UPG_COUNT);
    for (int i = 0; i < ITEM_COUNT; i++) {
        CHECK(shop_item(i)->price > 0);
        CHECK(shop_item(i)->max_level >= 1);
    }
}

TEST(camp_goods_spend_run_ore_and_grant_at_once)
{
    Profile p; RunState rs;
    fresh(&p, &rs);
    CHECK_EQ(shop_balance(SHOP_CAMP, &p, &rs), 100);
    CHECK(shop_can_buy(SHOP_CAMP, &p, &rs, ITEM_KEY));
    CHECK(shop_buy(SHOP_CAMP, &p, &rs, ITEM_KEY));
    CHECK_EQ(rs.hints, 4);
    CHECK_EQ(rs.ore, 60);
    CHECK(shop_buy(SHOP_CAMP, &p, &rs, ITEM_ROPE));
    CHECK_EQ(rs.rope_rooms, ROPE_ROOMS);
    CHECK_EQ(rs.ore, 10);
    CHECK(shop_sold_out(&p, &rs, ITEM_ROPE));      // one rope at a time
    rs.lives = 1;
    CHECK(!shop_can_buy(SHOP_CAMP, &p, &rs, ITEM_POTION));   // 60 > 10
    CHECK(!shop_buy(SHOP_CAMP, &p, &rs, ITEM_POTION));
    CHECK_EQ(rs.lives, 1);
    CHECK_EQ(p.ore_bank, 1000);                                // the bank is untouched at camp
    // camp goods keep a flat price
    CHECK_EQ(shop_price(ITEM_KEY, 0), 40);
    // permanent gear is not sold at camp and camp goods not at the counter
    CHECK(!shop_can_buy(SHOP_CAMP, &p, &rs, ITEM_BEER));
    CHECK(!shop_can_buy(SHOP_META, &p, &rs, ITEM_KEY));
    CHECK(!shop_can_buy(SHOP_CAMP, &p, NULL, ITEM_KEY));
}

TEST(camp_goods_sell_out_at_their_caps)
{
    Profile p; RunState rs;
    fresh(&p, &rs);
    rs.ore = 9999;
    rs.lives = rs.max_lives;                       // a potion only heals up to the run's maximum
    CHECK(shop_sold_out(&p, &rs, ITEM_POTION));
    rs.lives = 1;
    CHECK(!shop_sold_out(&p, &rs, ITEM_POTION));
    CHECK(shop_buy(SHOP_CAMP, &p, &rs, ITEM_POTION));
    CHECK_EQ(rs.lives, 2);
    rs.lives = rs.max_lives;
    CHECK(!shop_buy(SHOP_CAMP, &p, &rs, ITEM_POTION));
    for (int i = 0; i < 6; i++) shop_buy(SHOP_CAMP, &p, &rs, ITEM_KEY);
    CHECK_EQ(rs.hints, 9);
    CHECK(shop_sold_out(&p, &rs, ITEM_KEY));
    CHECK(shop_buy(SHOP_CAMP, &p, &rs, ITEM_ROPE));
    CHECK(!shop_buy(SHOP_CAMP, &p, &rs, ITEM_ROPE));
    rs.rope_rooms = 0;                             // used up: sold again
    CHECK(shop_buy(SHOP_CAMP, &p, &rs, ITEM_ROPE));
}

TEST(counter_gear_costs_more_per_level_and_is_remembered)
{
    Profile p; RunState rs;
    fresh(&p, &rs);
    CHECK_EQ(shop_price(ITEM_BEER, 0), 120);
    CHECK_EQ(shop_price(ITEM_BEER, 1), 240);
    CHECK_EQ(shop_price(ITEM_BEER, 3), 480);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BEER));
    CHECK_EQ(p.upgrade[UPG_BEER], 1);
    CHECK_EQ(p.ore_bank, 880);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BEER));
    CHECK_EQ(p.upgrade[UPG_BEER], 2);
    CHECK_EQ(p.ore_bank, 640);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BEER));          // 360: 280 left
    CHECK(!shop_buy(SHOP_META, &p, NULL, ITEM_BEER));         // 480 > 280
    p.ore_bank = 480;
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BEER));
    CHECK_EQ(p.upgrade[UPG_BEER], 4);
    CHECK(shop_sold_out(&p, NULL, ITEM_BEER));
    CHECK(!shop_buy(SHOP_META, &p, NULL, ITEM_BEER));
    CHECK_EQ(shop_max_lives(&p), 5);
    p.ore_bank = 79;
    CHECK(!shop_can_buy(SHOP_META, &p, NULL, ITEM_KEYS));
    p.ore_bank = 80;
    CHECK(shop_can_buy(SHOP_META, &p, NULL, ITEM_KEYS));
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_KEYS));
    CHECK_EQ(p.ore_bank, 0);
    CHECK_EQ(shop_start_hints(&p), 2);
}

TEST(gear_shapes_a_new_run)
{
    Profile p; RunState rs;
    fresh(&p, &rs);
    // starting lives need the room: bread is locked until beer raises the maximum
    CHECK(shop_locked(&p, ITEM_BREAD));
    CHECK(!shop_can_buy(SHOP_META, &p, NULL, ITEM_BREAD));
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BEER));
    CHECK_EQ(shop_max_lives(&p), 2);
    CHECK(!shop_locked(&p, ITEM_BREAD));
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BREAD));
    CHECK_EQ(shop_start_lives(&p), 2);
    CHECK(shop_locked(&p, ITEM_BREAD));            // a third starting life needs three max
    CHECK_EQ(p.ore_bank, 680);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BOOTS));
    CHECK_EQ(shop_time_bonus_pct(&p), 50);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_HELMET));
    CHECK_EQ(shop_helmet_pct(&p), 15);
    CHECK_EQ(p.ore_bank, 130);
    CHECK_EQ(shop_item_icon(&p, ITEM_HELMET), shop_item(ITEM_HELMET)->icon);
    RunState fresh_run;
    memset(&fresh_run, 0, sizeof fresh_run);
    fresh_run.lives = fresh_run.max_lives = 1;
    fresh_run.hints = 1;
    shop_apply_gear(&p, &fresh_run);
    CHECK_EQ(fresh_run.lives, 2);
    CHECK_EQ(fresh_run.max_lives, 2);
    CHECK_EQ(fresh_run.hints, 1);
    CHECK_EQ(fresh_run.time_bonus_pct, 50);
    CHECK_EQ(fresh_run.helmet_pct, 15);
    p.upgrade[UPG_BOOTS] = 3;
    CHECK_EQ(shop_time_bonus_pct(&p), 150);
    p.upgrade[UPG_HELMET] = 3;
    CHECK_EQ(shop_helmet_pct(&p), 45);
    CHECK(shop_item_icon(&p, ITEM_HELMET) != shop_item(ITEM_HELMET)->icon);   // golden at the last level
    p.upgrade[UPG_BEER] = 4;
    p.upgrade[UPG_BREAD] = 4;
    CHECK_EQ(shop_max_lives(&p), 5);
    CHECK_EQ(shop_start_lives(&p), 5);
    p.upgrade[UPG_BREAD] = 6;                      // never above the maximum
    p.upgrade[UPG_BEER] = 2;
    CHECK_EQ(shop_start_lives(&p), 3);
    p.upgrade[UPG_KEYS] = 5;
    shop_apply_gear(&p, &fresh_run);
    CHECK_EQ(fresh_run.hints, 6);
    CHECK_EQ(fresh_run.lives, 3);
}

const TestCase shop_tests[] = {
    T(catalogue_is_split_between_camp_and_counter),
    T(camp_goods_spend_run_ore_and_grant_at_once),
    T(camp_goods_sell_out_at_their_caps),
    T(counter_gear_costs_more_per_level_and_is_remembered),
    T(gear_shapes_a_new_run),
};
SUITE(shop)
