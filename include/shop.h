// The shop: what the merchant dwarf sells and what buying does. Pure logic
// (host-tested); source/shopscreen.c draws it.
//
// Two counters: at a camp, during a run, goods are paid with the run's ore and
// help right now; between runs (title menu), the ore brought back over every
// run (Profile.ore_bank) buys permanent gear and cosmetics.
#ifndef SHOP_H
#define SHOP_H

#include "common.h"
#include "run.h"
#include "save.h"

enum ShopMode { SHOP_CAMP = 0, SHOP_META };

enum ShopItem {
    // camp goods (run ore)
    ITEM_HINT = 0,       // +1 hint token
    ITEM_LIFE,           // +1 life
    ITEM_PROP,           // a prop: +2 stability in the next room
    // permanent gear (ore bank)
    ITEM_SATCHEL,        // +1 starting hint per level (2 levels)
    ITEM_BEDROLL,        // +1 maximum life per level (4 levels: 1 -> 5)
    ITEM_FLASK,          // +1 starting life per level (2 levels: 1 -> 3), needs the room in the maximum
    ITEM_LANTERN,        // more time in every room: +50 / +100 / +150 %
    ITEM_HELMET,         // cosmetic: golden helmet
    ITEM_BEARD,          // cosmetic: red beard
    ITEM_COUNT
};

#define SHOP_CAMP_FIRST ITEM_HINT
#define SHOP_CAMP_COUNT 3
#define SHOP_META_FIRST ITEM_SATCHEL
#define SHOP_META_COUNT 6

// Upgrade slots in Profile.upgrade[]
enum { UPG_SATCHEL = 0, UPG_FLASK, UPG_LANTERN, UPG_BEDROLL, UPG_COUNT };
// Cosmetic bits in Profile.cosmetics
enum { COS_HELMET = 1, COS_BEARD = 2 };

typedef struct {
    int  name_str, desc_str;
    int  price;              // for level 0; later levels cost more (shop_price)
    int  max_level;
} ShopItemInfo;

const ShopItemInfo *shop_item(int item);
int  shop_first(int mode);
int  shop_count(int mode);
// How many the player already holds (camp goods: current stock / lives / hints)
int  shop_level(const Profile *p, const RunState *rs, int item);
int  shop_price(int item, int level);
// Balance the mode spends from
int  shop_balance(int mode, const Profile *p, const RunState *rs);
bool shop_sold_out(const Profile *p, const RunState *rs, int item);
// Gear whose next level needs another purchase first (starting lives need max lives)
bool shop_locked(const Profile *p, int item);
// Lives a fresh run starts with / may hold, from the gear alone
int  shop_start_lives(const Profile *p);
int  shop_max_lives(const Profile *p);
int  shop_time_bonus_pct(const Profile *p);
bool shop_can_buy(int mode, const Profile *p, const RunState *rs, int item);
// Applies the purchase (deducts, grants). False when refused.
bool shop_buy(int mode, Profile *p, RunState *rs, int item);

// Effects of the permanent gear on a fresh run
void shop_apply_gear(const Profile *p, RunState *rs);

#endif
