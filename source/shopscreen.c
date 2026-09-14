#include "shopscreen.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "sound.h"
#include "rng.h"

// Layout (tiles): merchant sprite at the top left, his line beside him,
// goods from row 7, the highlighted good's description at 15-16, balance
// and keys at the bottom.
#define MERCHANT_X   12                 // 64x64 animated sprite, centred in the left column (88 px)
#define MERCHANT_Y   40
#define LINE_TX      10
#define LIST_TY      7
#define LIST_NAME_TX 11
#define LIST_PRICE_TX 24
#define DESC_TY      15
#define DESC_TX      5                  // after the good's 16 px icon
#define STATUS_TY    18

static int mode, cursor;
static Profile *prof;
static RunState *run_state;
static bool bought;
static int say_timer;

static void say(int str_id, int pal)
{
    txt_clear_rect(LINE_TX, 2, TILES_W - LINE_TX, 3);
    // long lines wrap onto two rows at a space
    const char *s = S(str_id);
    int width = TILES_W - LINE_TX - 1;
    int len = txt_len(s);
    if (len <= width) { txt_puts(LINE_TX, 2, s, pal); return; }
    int cut = width;
    while (cut > 0 && s[cut] != ' ') cut--;
    if (cut == 0) cut = width;
    char first[40];
    for (int i = 0; i < cut && i < 39; i++) first[i] = s[i];
    first[cut < 39 ? cut : 39] = 0;
    txt_puts(LINE_TX, 2, first, pal);
    txt_puts(LINE_TX, 3, s + cut + 1, pal);
    say_timer = 0;
}

static void draw_goods(void)
{
    int first = shop_first(mode), count = shop_count(mode);
    txt_clear_rect(LIST_NAME_TX - 2, LIST_TY, TILES_W - LIST_NAME_TX + 2, count + 1);
    for (int i = 0; i < count; i++) {
        int item = first + i;
        const ShopItemInfo *it = shop_item(item);
        int level = shop_level(prof, run_state, item);
        bool out = shop_sold_out(prof, run_state, item);
        bool can = shop_can_buy(mode, prof, run_state, item);
        int row = LIST_TY + i;
        int pal = out ? PAL_TXT_GRAY : i == cursor ? PAL_TXT_GOLD : can ? PAL_TXT_WHITE : PAL_TXT_GRAY;
        if (i == cursor) txt_puts(LIST_NAME_TX - 2, row, ">", PAL_TXT_GOLD);
        txt_puts(LIST_NAME_TX, row, S(it->name_str), pal);
        if (it->max_level > 1 && mode == SHOP_META && level > 0 && !out) {
            txt_puts(LIST_NAME_TX + txt_len(S(it->name_str)) + 1, row, "+", pal);
            txt_putint(LIST_NAME_TX + txt_len(S(it->name_str)) + 2, row, level, pal);
        }
        if (out) {
            txt_puts(LIST_PRICE_TX, row, S(STR_SOLD), PAL_TXT_GRAY);
        } else {
            int price = shop_price(item, mode == SHOP_META ? level : 0);
            txt_putint(LIST_PRICE_TX, row, price, can ? PAL_TXT_GOLD : PAL_TXT_RED);
            txt_puts(LIST_PRICE_TX + (price >= 100 ? 3 : price >= 10 ? 2 : 1) + 1, row, "M", PAL_TXT_GRAY);
        }
    }
    // the highlighted good: its icon, then its description beside it
    txt_clear_rect(0, DESC_TY, TILES_W, 2);
    map_icon(DESC_TX - 3, DESC_TY, ICON_SHOP_FIRST + (first + cursor), PAL_NODE_LIT);
    const char *d = S(shop_item(first + cursor)->desc_str);
    int width = TILES_W - DESC_TX - 1, len = txt_len(d);
    if (len <= width) { txt_puts(DESC_TX, DESC_TY, d, PAL_TXT_WHITE); return; }
    int cut = width;
    while (cut > 0 && d[cut] != ' ') cut--;
    char a[40];
    for (int i = 0; i < cut && i < 39; i++) a[i] = d[i];
    a[cut < 39 ? cut : 39] = 0;
    txt_puts(DESC_TX, DESC_TY, a, PAL_TXT_WHITE);
    txt_puts(DESC_TX, DESC_TY + 1, d + cut + 1, PAL_TXT_WHITE);
}

static void draw_status(void)
{
    txt_clear_rect(0, STATUS_TY, TILES_W, 2);
    txt_puts(10, STATUS_TY, S(STR_ORE), PAL_TXT_GRAY);
    txt_putint(10 + txt_len(S(STR_ORE)) + 1, STATUS_TY, shop_balance(mode, prof, run_state), PAL_TXT_GOLD);
    if (mode == SHOP_META) txt_puts(23, STATUS_TY, S(STR_BANK), PAL_TXT_GRAY);
}

void shop_enter(int m, Profile *p, RunState *rs)
{
    mode = m;
    prof = p;
    run_state = rs;
    cursor = 0;
    bought = false;
    render_clear();
    render_palettes_map();                        // the icon bank
    music_play(MUS_MAP);
    txt_puts_center(0, S(STR_SHOP_TITLE), PAL_TXT_GOLD);
    merchant_set(MERCHANT_X, MERCHANT_Y, true);   // his sprite brings its own stall
    static const int greetings[3] = { STR_SHOP_HELLO_1, STR_SHOP_HELLO_2, STR_SHOP_HELLO_3 };
    say(mode == SHOP_CAMP ? greetings[rng_range(3)] : STR_SHOP_HELLO_META, PAL_TXT_WHITE);
    draw_goods();
    draw_status();
}

int shop_update(void)
{
    int count = shop_count(mode);
    if (input_hit(KEY_UP | KEY_DOWN)) {
        cursor = (cursor + (input_hit(KEY_UP) ? -1 : 1) + count) % count;
        sfx_play(SFX_MOVE);
        draw_goods();
    }
    if (input_hit(KEY_A)) {
        int item = shop_first(mode) + cursor;
        if (shop_buy(mode, prof, run_state, item)) {
            bought = true;
            sfx_play(SFX_COLLECT);
            say(STR_SHOP_THANKS, PAL_TXT_GOLD);
            draw_goods();
            draw_status();
        } else {
            sfx_play(SFX_ERROR);
            say(shop_sold_out(prof, run_state, item) ? STR_SHOP_SOLD_OUT : STR_SHOP_TOO_POOR, PAL_TXT_RED);
        }
    }
    if (input_hit(KEY_B | KEY_START)) {
        sfx_play(SFX_MARK);
        return SHOPSCREEN_LEAVE;
    }
    return SHOPSCREEN_RUNNING;
}

bool shop_bought_something(void) { return bought; }
