#include "shop.h"
#include "lang.h"

static const ShopItemInfo items[ITEM_COUNT] = {
    [ITEM_HINT]    = { STR_ITEM_HINT,    STR_ITEM_HINT_DESC,    30,  9 },
    [ITEM_LIFE]    = { STR_ITEM_LIFE,    STR_ITEM_LIFE_DESC,    60,  RUN_MAX_LIVES },
    [ITEM_PROP]    = { STR_ITEM_PROP,    STR_ITEM_PROP_DESC,    25,  3 },
    [ITEM_SATCHEL] = { STR_ITEM_SATCHEL, STR_ITEM_SATCHEL_DESC, 200, 2 },
    [ITEM_FLASK]   = { STR_ITEM_FLASK,   STR_ITEM_FLASK_DESC,   400, 1 },
    [ITEM_LANTERN] = { STR_ITEM_LANTERN, STR_ITEM_LANTERN_DESC, 300, 1 },
    [ITEM_HELMET]  = { STR_ITEM_HELMET,  STR_ITEM_HELMET_DESC,  150, 1 },
    [ITEM_BEARD]   = { STR_ITEM_BEARD,   STR_ITEM_BEARD_DESC,   150, 1 },
};

const ShopItemInfo *shop_item(int item) { return &items[clampi(item, 0, ITEM_COUNT - 1)]; }
int shop_first(int mode) { return mode == SHOP_CAMP ? SHOP_CAMP_FIRST : SHOP_META_FIRST; }
int shop_count(int mode) { return mode == SHOP_CAMP ? SHOP_CAMP_COUNT : SHOP_META_COUNT; }

int shop_level(const Profile *p, const RunState *rs, int item)
{
    switch (item) {
    case ITEM_HINT:    return rs ? rs->hints : 0;
    case ITEM_LIFE:    return rs ? rs->lives : 0;
    case ITEM_PROP:    return rs ? rs->props : 0;
    case ITEM_SATCHEL: return p->upgrade[UPG_SATCHEL];
    case ITEM_FLASK:   return p->upgrade[UPG_FLASK];
    case ITEM_LANTERN: return p->upgrade[UPG_LANTERN];
    case ITEM_HELMET:  return (p->cosmetics & COS_HELMET) ? 1 : 0;
    case ITEM_BEARD:   return (p->cosmetics & COS_BEARD) ? 1 : 0;
    default:           return 0;
    }
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
    return shop_level(p, rs, item) >= shop_item(item)->max_level;
}

bool shop_can_buy(int mode, const Profile *p, const RunState *rs, int item)
{
    int first = shop_first(mode);
    if (item < first || item >= first + shop_count(mode)) return false;
    if (mode == SHOP_CAMP && !rs) return false;
    if (shop_sold_out(p, rs, item)) return false;
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
    case ITEM_HINT:    rs->hints++; break;
    case ITEM_LIFE:    rs->lives++; break;
    case ITEM_PROP:    rs->props++; break;
    case ITEM_SATCHEL: p->upgrade[UPG_SATCHEL]++; break;
    case ITEM_FLASK:   p->upgrade[UPG_FLASK]++; break;
    case ITEM_LANTERN: p->upgrade[UPG_LANTERN]++; break;
    case ITEM_HELMET:  p->cosmetics |= COS_HELMET; break;
    case ITEM_BEARD:   p->cosmetics |= COS_BEARD; break;
    default: break;
    }
    return true;
}

void shop_apply_gear(const Profile *p, RunState *rs)
{
    rs->hints = (u8)clampi(rs->hints + p->upgrade[UPG_SATCHEL], 0, 9);
    rs->lives = (u8)clampi(rs->lives + p->upgrade[UPG_FLASK], 0, RUN_MAX_LIVES);
    rs->max_lives = (u8)clampi(rs->max_lives + p->upgrade[UPG_FLASK], 0, RUN_MAX_LIVES);
    rs->time_bonus_pct = (u8)(p->upgrade[UPG_LANTERN] ? 25 : 0);
}
