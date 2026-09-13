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
    CHECK_EQ(shop_first(SHOP_CAMP), ITEM_HINT);
    CHECK_EQ(shop_count(SHOP_CAMP), 3);
    CHECK_EQ(shop_first(SHOP_META), ITEM_SATCHEL);
    CHECK_EQ(shop_count(SHOP_META), 5);
    CHECK_EQ(shop_first(SHOP_META) + shop_count(SHOP_META), ITEM_COUNT);
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
    CHECK(shop_can_buy(SHOP_CAMP, &p, &rs, ITEM_HINT));
    CHECK(shop_buy(SHOP_CAMP, &p, &rs, ITEM_HINT));
    CHECK_EQ(rs.hints, 4);
    CHECK_EQ(rs.ore, 70);
    CHECK(shop_buy(SHOP_CAMP, &p, &rs, ITEM_PROP));
    CHECK_EQ(rs.props, 1);
    CHECK_EQ(rs.ore, 45);
    CHECK(!shop_can_buy(SHOP_CAMP, &p, &rs, ITEM_LIFE));      // 60 > 45
    CHECK(!shop_buy(SHOP_CAMP, &p, &rs, ITEM_LIFE));
    CHECK_EQ(rs.lives, 3);
    CHECK_EQ(p.ore_bank, 1000);                                // the bank is untouched at camp
    // camp goods keep a flat price
    CHECK_EQ(shop_price(ITEM_HINT, 0), 30);
    // permanent gear is not sold at camp and camp goods not at the counter
    CHECK(!shop_can_buy(SHOP_CAMP, &p, &rs, ITEM_SATCHEL));
    CHECK(!shop_can_buy(SHOP_META, &p, &rs, ITEM_HINT));
    CHECK(!shop_can_buy(SHOP_CAMP, &p, NULL, ITEM_HINT));
}

TEST(camp_goods_sell_out_at_their_caps)
{
    Profile p; RunState rs;
    fresh(&p, &rs);
    rs.ore = 9999;
    rs.lives = RUN_MAX_LIVES;
    CHECK(shop_sold_out(&p, &rs, ITEM_LIFE));
    CHECK(!shop_buy(SHOP_CAMP, &p, &rs, ITEM_LIFE));
    for (int i = 0; i < 6; i++) shop_buy(SHOP_CAMP, &p, &rs, ITEM_HINT);
    CHECK_EQ(rs.hints, 9);
    CHECK(shop_sold_out(&p, &rs, ITEM_HINT));
    for (int i = 0; i < 5; i++) shop_buy(SHOP_CAMP, &p, &rs, ITEM_PROP);
    CHECK_EQ(rs.props, 3);
}

TEST(counter_gear_costs_more_per_level_and_is_remembered)
{
    Profile p; RunState rs;
    fresh(&p, &rs);
    CHECK_EQ(shop_price(ITEM_SATCHEL, 0), 200);
    CHECK_EQ(shop_price(ITEM_SATCHEL, 1), 400);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_SATCHEL));
    CHECK_EQ(p.upgrade[UPG_SATCHEL], 1);
    CHECK_EQ(p.ore_bank, 800);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_SATCHEL));
    CHECK_EQ(p.upgrade[UPG_SATCHEL], 2);
    CHECK_EQ(p.ore_bank, 400);
    CHECK(shop_sold_out(&p, NULL, ITEM_SATCHEL));
    CHECK(!shop_buy(SHOP_META, &p, NULL, ITEM_SATCHEL));
    p.ore_bank = 399;
    CHECK(!shop_can_buy(SHOP_META, &p, NULL, ITEM_FLASK));
    p.ore_bank = 400;
    CHECK(shop_can_buy(SHOP_META, &p, NULL, ITEM_FLASK));
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_HELMET));
    CHECK_EQ(p.ore_bank, 250);
}

TEST(cosmetics_are_bits_and_gear_shapes_a_new_run)
{
    Profile p; RunState rs;
    fresh(&p, &rs);
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_HELMET));
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_BEARD));
    CHECK_EQ(p.cosmetics, COS_HELMET | COS_BEARD);
    CHECK(shop_sold_out(&p, NULL, ITEM_HELMET));
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_FLASK));
    CHECK(shop_buy(SHOP_META, &p, NULL, ITEM_LANTERN));
    CHECK_EQ(p.ore_bank, 0);
    RunState fresh_run;
    memset(&fresh_run, 0, sizeof fresh_run);
    fresh_run.lives = fresh_run.max_lives = 3;
    fresh_run.hints = 3;
    shop_apply_gear(&p, &fresh_run);
    CHECK_EQ(fresh_run.lives, 4);
    CHECK_EQ(fresh_run.max_lives, 4);
    CHECK_EQ(fresh_run.hints, 3);
    CHECK_EQ(fresh_run.time_bonus_pct, 25);
    p.upgrade[UPG_SATCHEL] = 2;
    shop_apply_gear(&p, &fresh_run);
    CHECK_EQ(fresh_run.hints, 5);
}

const TestCase shop_tests[] = {
    T(catalogue_is_split_between_camp_and_counter),
    T(camp_goods_spend_run_ore_and_grant_at_once),
    T(camp_goods_sell_out_at_their_caps),
    T(counter_gear_costs_more_per_level_and_is_remembered),
    T(cosmetics_are_bits_and_gear_shapes_a_new_run),
};
SUITE(shop)
