#include "shop.h"
#include "lang.h"
#include "gfx_icons.h"

static const ShopItemInfo items[ITEM_COUNT] = {
    [ITEM_POTION] = { STR_ITEM_POTION, STR_ITEM_POTION_DESC, 60,  RUN_MAX_LIVES, ICON_POTION },
    [ITEM_KEY]    = { STR_ITEM_KEY,    STR_ITEM_KEY_DESC,    40,  9,             ICON_KEY },
    [ITEM_ROPE]   = { STR_ITEM_ROPE,   STR_ITEM_ROPE_DESC,   50,  1,             ICON_ROPES },
    [ITEM_BEER]   = { STR_ITEM_BEER,   STR_ITEM_BEER_DESC,   120, 4,             ICON_BEER },
    [ITEM_BREAD]  = { STR_ITEM_BREAD,  STR_ITEM_BREAD_DESC,  200, 4,             ICON_BREAD },
    [ITEM_HELMET] = { STR_ITEM_HELMET, STR_ITEM_HELMET_DESC, 250, 3,             ICON_SIMPLEHELMET },
    [ITEM_KEYS]   = { STR_ITEM_KEYS,   STR_ITEM_KEYS_DESC,   80,  5,             ICON_KEY },
    [ITEM_BOOTS]  = { STR_ITEM_BOOTS,  STR_ITEM_BOOTS_DESC,  300, 3,             ICON_BOOTS },
};

const ShopItemInfo *shop_item(int item) { return &items[clampi(item, 0, ITEM_COUNT - 1)]; }
int shop_first(int mode) { return mode == SHOP_CAMP ? SHOP_CAMP_FIRST : SHOP_META_FIRST; }
int shop_count(int mode) { return mode == SHOP_CAMP ? SHOP_CAMP_COUNT : SHOP_META_COUNT; }

int shop_level(const Profile *p, const RunState *rs, int item)
{
    switch (item) {
    case ITEM_POTION: return rs ? rs->lives : 0;
    case ITEM_KEY:    return rs ? rs->hints : 0;
    case ITEM_ROPE:   return rs && rs->rope_rooms ? 1 : 0;
    case ITEM_BEER:   return p->upgrade[UPG_BEER];
    case ITEM_BREAD:  return p->upgrade[UPG_BREAD];
    case ITEM_HELMET: return p->upgrade[UPG_HELMET];
    case ITEM_KEYS:   return p->upgrade[UPG_KEYS];
    case ITEM_BOOTS:  return p->upgrade[UPG_BOOTS];
    default:          return 0;
    }
}

int shop_item_icon(const Profile *p, int item)
{
    if (item == ITEM_HELMET && p->upgrade[UPG_HELMET] >= 3) return ICON_GOLDHELMET;
    return shop_item(item)->icon;
}

int shop_price(int item, int level)
{
    const ShopItemInfo *it = shop_item(item);
    return it->price * (level + 1);              // each further level doubles, triples...
}

int shop_balance(int mode, const Profile *p, const RunState *rs)
{
    return mode == SHOP_CAMP ? (rs ? rs->ore : 0) : p->ore_bank;
}

bool shop_sold_out(const Profile *p, const RunState *rs, int item)
{
    if (item == ITEM_POTION) return !rs || rs->lives >= rs->max_lives;   // a camp heals up to the run's maximum
    return shop_level(p, rs, item) >= shop_item(item)->max_level;
}

int shop_max_lives(const Profile *p)   { return clampi(1 + p->upgrade[UPG_BEER], 1, RUN_MAX_LIVES); }
int shop_start_lives(const Profile *p) { return clampi(1 + p->upgrade[UPG_BREAD], 1, shop_max_lives(p)); }
int shop_start_hints(const Profile *p) { return clampi(1 + p->upgrade[UPG_KEYS], 1, 9); }
int shop_helmet_pct(const Profile *p)  { return 15 * clampi(p->upgrade[UPG_HELMET], 0, 3); }
int shop_time_bonus_pct(const Profile *p)
{
    static const int pct[4] = { 0, 50, 100, 150 };
    return pct[clampi(p->upgrade[UPG_BOOTS], 0, 3)];
}

bool shop_locked(const Profile *p, int item)
{
    // the next loaf would start the run with more lives than the maximum allows
    if (item == ITEM_BREAD) return 1 + p->upgrade[UPG_BREAD] + 1 > shop_max_lives(p);
    return false;
}

bool shop_can_buy(int mode, const Profile *p, const RunState *rs, int item)
{
    int first = shop_first(mode);
    if (item < first || item >= first + shop_count(mode)) return false;
    if (mode == SHOP_CAMP && !rs) return false;
    if (shop_sold_out(p, rs, item)) return false;
    if (mode == SHOP_META && shop_locked(p, item)) return false;
    // permanent gear is priced per level; camp goods keep their price
    int level = mode == SHOP_META ? shop_level(p, rs, item) : 0;
    return shop_balance(mode, p, rs) >= shop_price(item, level);
}

bool shop_buy(int mode, Profile *p, RunState *rs, int item)
{
    if (!shop_can_buy(mode, p, rs, item)) return false;
    int level = mode == SHOP_META ? shop_level(p, rs, item) : 0;
    int price = shop_price(item, level);
    if (mode == SHOP_CAMP) rs->ore = (u16)(rs->ore - price);
    else                   p->ore_bank = (u16)(p->ore_bank - price);
    switch (item) {
    case ITEM_POTION: rs->lives++; break;
    case ITEM_KEY:    rs->hints++; break;
    case ITEM_ROPE:   rs->rope_rooms = ROPE_ROOMS; break;
    case ITEM_BEER:   p->upgrade[UPG_BEER]++; break;
    case ITEM_BREAD:  p->upgrade[UPG_BREAD]++; break;
    case ITEM_HELMET: p->upgrade[UPG_HELMET]++; break;
    case ITEM_KEYS:   p->upgrade[UPG_KEYS]++; break;
    case ITEM_BOOTS:  p->upgrade[UPG_BOOTS]++; break;
    default: break;
    }
    return true;
}

void shop_apply_gear(const Profile *p, RunState *rs)
{
    // the run starts at one life of one and one hint: beer raises the maximum,
    // bread the starting count (never above the maximum), keys the hints
    rs->max_lives = (u8)shop_max_lives(p);
    rs->lives = (u8)shop_start_lives(p);
    rs->hints = (u8)shop_start_hints(p);
    rs->helmet_pct = (u8)shop_helmet_pct(p);
    rs->time_bonus_pct = (u8)shop_time_bonus_pct(p);
}
