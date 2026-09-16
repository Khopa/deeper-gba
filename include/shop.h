// The shop: what the merchant dwarf sells and what buying does. Pure logic
// (host-tested); source/shopscreen.c draws it.
//
// Two counters: at a camp, during a run, goods are paid with the run's ore and
// help right now; between runs (title menu), the ore brought back over every
// run (Profile.ore_bank) buys permanent gear, level by level.
#ifndef SHOP_H
#define SHOP_H

#include "common.h"
#include "run.h"
#include "save.h"

enum ShopMode { SHOP_CAMP = 0, SHOP_META };

enum ShopItem {
    // camp goods (run ore)
    ITEM_POTION = 0,     // +1 life (up to the run's maximum)
    ITEM_KEY,            // +1 hint token
    ITEM_ROPE,           // +2 stability in the next three rooms
    // permanent gear (ore bank)
    ITEM_BEER,           // +1 maximum life per level (4 levels: 1 -> 5)
    ITEM_BREAD,          // +1 starting life per level (4 levels: 1 -> 5), never above the maximum
    ITEM_HELMET,         // a blow costs no life 15 / 30 / 45 % of the time; monsters land fewer hits
    ITEM_KEYS,           // +1 starting hint per level (5 levels)
    ITEM_BOOTS,          // more time in every room: +50 / +100 / +150 %
    ITEM_COUNT
};

#define SHOP_CAMP_FIRST ITEM_POTION
#define SHOP_CAMP_COUNT 3
#define SHOP_META_FIRST ITEM_BEER
#define SHOP_META_COUNT 5
#define ROPE_ROOMS      3

// Gear slots in Profile.upgrade[] (SHOP_META order)
enum Upgrade { UPG_BEER = 0, UPG_BREAD, UPG_HELMET, UPG_KEYS, UPG_BOOTS, UPG_COUNT };

typedef struct {
    int  name_str, desc_str;
    int  price;              // for level 0; later levels cost more (shop_price)
    int  max_level;
    int  icon;               // enum Icon (gfx_icons.h)
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
// What the gear gives a fresh run
int  shop_start_lives(const Profile *p);
int  shop_max_lives(const Profile *p);
int  shop_start_hints(const Profile *p);
int  shop_helmet_pct(const Profile *p);
int  shop_time_bonus_pct(const Profile *p);
int  shop_item_icon(const Profile *p, int item);   // the helmet turns golden at its last level
bool shop_can_buy(int mode, const Profile *p, const RunState *rs, int item);
// Applies the purchase (deducts, grants). False when refused.
bool shop_buy(int mode, Profile *p, RunState *rs, int item);

// Effects of the permanent gear on a fresh run
void shop_apply_gear(const Profile *p, RunState *rs);

#endif
