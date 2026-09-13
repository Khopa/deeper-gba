// Deeper — entry point and top-level screen flow:
//   title menu -> (continue | new run) -> map <-> rooms ... -> run end -> title
// The run block in SRAM is refreshed after every room outcome and after
// every board change inside a room, so a run resumes exactly where it was.
#include "common.h"
#include "input.h"
#include "render.h"
#include "lang.h"
#include "bank.h"
#include "room.h"
#include "run.h"
#include "mapscreen.h"
#include "save.h"
#include "rng.h"
#include "sound.h"
#include "biome.h"
#include "shop.h"
#include "shopscreen.h"

enum { SCR_TITLE, SCR_RECORDS, SCR_MAP, SCR_ROOM, SCR_RUN_END, SCR_LANG, SCR_LENGTH, SCR_SHOP };
enum { MENU_CONTINUE, MENU_NEW, MENU_SHOP, MENU_RECORDS, MENU_COUNT };

// Not static: the emulator scenarios (tests/emu) read them from RAM.
int screen;
RunState run;
u32 frames;
int menu_cursor;
int lang_cursor;
int length_cursor;              // descent length picked on the length screen
u32 debug_seed;                 // when non-zero, the next run uses it (emulator scenarios)
static u32 new_powers;
static bool new_length, new_record;
static int shop_mode;

// --- title ----------------------------------------------------------------------------

static bool menu_enabled(int item) { return item != MENU_CONTINUE || save_has_run(); }

static void title_draw_menu(void)
{
    static const int labels[MENU_COUNT] = { STR_CONTINUE, STR_NEW_RUN, STR_SHOP, STR_RECORDS };
    for (int i = 0; i < MENU_COUNT; i++) {
        int pal = !menu_enabled(i) ? PAL_TXT_GRAY : i == menu_cursor ? PAL_TXT_GOLD : PAL_TXT_WHITE;
        txt_clear_rect(0, 10 + 2 * i, TILES_W, 1);
        txt_puts_center(10 + 2 * i, S(labels[i]), pal);
        if (i == menu_cursor) txt_puts((TILES_W - txt_len(S(labels[i]))) / 2 - 2, 10 + 2 * i, ">", PAL_TXT_GOLD);
    }
}

static void title_draw_sound(void)
{
    txt_clear_rect(0, 18, TILES_W, 1);
    char line[24];
    const char *label = S(STR_SOUND), *value = S(sound_enabled() ? STR_ON : STR_OFF);
    int i = 0;
    line[i++] = 'S'; line[i++] = 'E'; line[i++] = 'L'; line[i++] = ':';
    line[i++] = ' ';
    for (const char *p = label; *p && i < 20; p++) line[i++] = *p;
    line[i++] = ' ';
    for (const char *p = value; *p && i < 23; p++) line[i++] = *p;
    line[i] = 0;
    txt_puts_center(18, line, PAL_TXT_GRAY);
}

// Language pick, shown at every boot with the saved choice preselected.
static void lang_draw(void)
{
    for (int i = 0; i < LANG_COUNT; i++) {
        txt_clear_rect(0, 9 + 2 * i, TILES_W, 1);
        txt_puts_center(9 + 2 * i, lang_name(i), i == lang_cursor ? PAL_TXT_GOLD : PAL_TXT_WHITE);
        if (i == lang_cursor) txt_puts((TILES_W - txt_len(lang_name(i))) / 2 - 2, 9 + 2 * i, ">", PAL_TXT_GOLD);
    }
}

static void lang_enter(void)
{
    screen = SCR_LANG;
    render_clear();
    render_set_biome(BIOME_EARTH);
    txt_puts_center(5, S(STR_LANG_PROMPT), PAL_TXT_GOLD);
    lang_cursor = lang_get();
    lang_draw();
    dwarf_set(112, 120, true);
    dwarf_play(DWARF_IDLE);
}

static void title_enter(void)
{
    screen = SCR_TITLE;
    render_clear();
    render_set_biome(BIOME_EARTH);
    music_play(MUS_NONE);
    txt_puts_center(4, S(STR_TITLE), PAL_TXT_GOLD);
    title_draw_sound();
    dwarf_set(112, 52, true);
    dwarf_play(DWARF_IDLE);
    menu_cursor = save_has_run() ? MENU_CONTINUE : MENU_NEW;
    title_draw_menu();
}

// MM:SS from a frame count (60 fps), centred on a row or at a column
static void put_time(int tx, int ty, u32 frames, int pal)
{
    u32 secs = frames / 60;
    char buf[8];
    u32 m = secs / 60, s = secs % 60;
    if (m > 99) m = 99;
    buf[0] = (char)('0' + m / 10);
    buf[1] = (char)('0' + m % 10);
    buf[2] = ':';
    buf[3] = (char)('0' + s / 10);
    buf[4] = (char)('0' + s % 10);
    buf[5] = 0;
    if (tx < 0) txt_puts_center(ty, buf, pal); else txt_puts(tx, ty, buf, pal);
}

// Length pick: 15, 30 or 60 layers; longer descents unlock by reaching the core
static void length_draw(void)
{
    const Profile *p = save_profile();
    for (int i = 0; i < RUN_LENGTHS; i++) {
        bool open = i < p->lengths_unlocked;
        int row = 8 + 3 * i;
        txt_clear_rect(0, row, TILES_W, 2);
        int pal = !open ? PAL_TXT_GRAY : i == length_cursor ? PAL_TXT_GOLD : PAL_TXT_WHITE;
        int len = run_length(i);
        int x = (TILES_W - (3 + txt_len(S(STR_LAYERS)))) / 2;
        txt_putint(x, row, len, pal);
        txt_puts(x + 3, row, S(STR_LAYERS), pal);
        if (i == length_cursor) txt_puts(x - 2, row, ">", PAL_TXT_GOLD);
        if (!open) {
            txt_puts_center(row + 1, S(STR_UNLOCK_LENGTH), PAL_TXT_GRAY);
            int lx = (TILES_W + txt_len(S(STR_UNLOCK_LENGTH))) / 2 + 1;
            txt_putint(lx, row + 1, run_length(i - 1), PAL_TXT_GRAY);
        } else if (p->best_frames[i]) {
            txt_puts((TILES_W - 12) / 2, row + 1, S(STR_BEST_TIME), PAL_TXT_GRAY);
            put_time((TILES_W - 12) / 2 + 7, row + 1, p->best_frames[i], PAL_TXT_WHITE);
        }
    }
}

static void length_enter(void)
{
    screen = SCR_LENGTH;
    render_clear();
    txt_puts_center(4, S(STR_LENGTH_PROMPT), PAL_TXT_GOLD);
    length_cursor = clampi(length_cursor, 0, save_profile()->lengths_unlocked - 1);
    length_draw();
    txt_puts_center(18, S(STR_BACK), PAL_TXT_GRAY);
}

static void records_enter(void)
{
    screen = SCR_RECORDS;
    render_clear();
    const Profile *p = save_profile();
    txt_puts_center(1, S(STR_RECORDS), PAL_TXT_GOLD);
    txt_puts(2, 3, S(STR_RUNS), PAL_TXT_GRAY);
    txt_putint(20, 3, p->runs_started, PAL_TXT_WHITE);
    txt_puts(2, 4, S(STR_RUNS_WON), PAL_TXT_GRAY);
    txt_putint(20, 4, p->runs_won, PAL_TXT_WHITE);
    txt_puts(2, 5, S(STR_BEST_DEPTH), PAL_TXT_GRAY);
    txt_putint(20, 5, p->best_depth, PAL_TXT_WHITE);
    txt_puts(2, 6, S(STR_TOTAL_ORE), PAL_TXT_GRAY);
    txt_putint(20, 6, (int)p->total_ore, PAL_TXT_GOLD);
    // best times per descent length, last run time
    for (int i = 0; i < RUN_LENGTHS; i++) {
        txt_puts(2, 8 + i, S(STR_BEST_TIME), PAL_TXT_GRAY);
        txt_putint(9, 8 + i, run_length(i), PAL_TXT_GRAY);
        if (p->best_frames[i]) put_time(20, 8 + i, p->best_frames[i], PAL_TXT_WHITE);
        else txt_puts(20, 8 + i, i < p->lengths_unlocked ? "--:--" : S(STR_LOCKED), PAL_TXT_GRAY);
    }
    txt_puts(2, 11, S(STR_LAST_TIME), PAL_TXT_GRAY);
    if (p->last_frames) put_time(20, 11, p->last_frames, PAL_TXT_WHITE);

    txt_puts(2, 13, S(STR_POWERS), PAL_TXT_GOLD);
    static const int names[POWER_COUNT] = { STR_POWER_LAMP, STR_POWER_TOUGH, STR_POWER_SECOND };
    static const int howto[POWER_COUNT] = { STR_UNLOCK_LAMP, STR_UNLOCK_TOUGH, STR_UNLOCK_SECOND };
    for (int i = 0; i < POWER_COUNT; i++) {
        bool owned = (p->powers >> i) & 1;
        txt_puts(2, 14 + 2 * i, owned ? "*" : ".", owned ? PAL_TXT_GOLD : PAL_TXT_GRAY);
        txt_puts(4, 14 + 2 * i, S(names[i]), owned ? PAL_TXT_WHITE : PAL_TXT_GRAY);
        txt_puts(6, 15 + 2 * i, owned ? "" : S(howto[i]), PAL_TXT_GRAY);
    }
}

// --- run flow ----------------------------------------------------------------------------

static void map_enter_with(const char *msg, int pal)
{
    map_enter(&run);
    if (msg) txt_puts_center(18, msg, pal);
    else txt_puts_center(18, S(STR_MAP_HELP), PAL_TXT_GRAY);
    screen = SCR_MAP;
}

static void put_number_center(int ty, int v, int pal)
{
    char buf[8];
    int len = 0;
    if (v >= 1000) buf[len++] = (char)('0' + v / 1000);
    if (v >= 100) buf[len++] = (char)('0' + (v / 100) % 10);
    if (v >= 10) buf[len++] = (char)('0' + (v / 10) % 10);
    buf[len++] = (char)('0' + v % 10);
    buf[len] = 0;
    txt_puts_center(ty, buf, pal);
}

static void run_end_enter(bool won)
{
    screen = SCR_RUN_END;
    Profile *p = save_profile();
    if (won) p->runs_won++;
    if (run.layer + 1 > p->best_depth) p->best_depth = (u16)(run.layer + 1);
    p->total_ore += run.ore;
    p->ore_bank = (u16)clampi(p->ore_bank + run.ore, 0, 65535);
    p->last_frames = run.frames;
    p->total_frames += run.frames;
    new_record = new_length = false;
    if (won) {
        int li = run.length_index;
        if (!p->best_frames[li] || run.frames < p->best_frames[li]) { p->best_frames[li] = run.frames; new_record = true; }
        if (li + 1 < RUN_LENGTHS && p->lengths_unlocked <= (u8)(li + 1)) { p->lengths_unlocked = (u8)(li + 2); new_length = true; }
    }
    new_powers = save_check_unlocks();
    save_profile_commit();
    save_run_clear();

    render_clear();
    music_play(won ? MUS_VICTORY : MUS_DEFEAT);
    sfx_play(won ? SFX_SOLVED : SFX_COLLAPSE);
    if (new_powers) sfx_play(SFX_POWER);
    txt_puts_center(4, S(won ? STR_RUN_WON : STR_RUN_LOST), won ? PAL_TXT_GOLD : PAL_TXT_RED);
    txt_puts_center(7, S(STR_DEPTH), PAL_TXT_GRAY);
    put_number_center(8, run.layer + 1, PAL_TXT_WHITE);
    txt_puts(2, 10, S(STR_TOTAL_ORE), PAL_TXT_GRAY);
    txt_putint(2, 11, run.ore, PAL_TXT_GOLD);
    txt_puts(20, 10, S(STR_TIME), PAL_TXT_GRAY);
    put_time(20, 11, run.frames, new_record ? PAL_TXT_GOLD : PAL_TXT_WHITE);
    int row = 13;
    if (new_record) txt_puts_center(row++, S(STR_NEW_RECORD), PAL_TXT_GOLD);
    if (new_length) txt_puts_center(row++, S(STR_LENGTH_UNLOCKED), PAL_TXT_GOLD);
    if (new_powers) {
        txt_puts_center(row++, S(STR_NEW_POWER), PAL_TXT_GOLD);
        static const int names[POWER_COUNT] = { STR_POWER_LAMP, STR_POWER_TOUGH, STR_POWER_SECOND };
        for (int i = 0; i < POWER_COUNT; i++)
            if ((new_powers >> i) & 1) txt_puts_center(row++, S(names[i]), PAL_TXT_WHITE);
    }
    txt_puts_center(18, S(STR_PRESS_START), PAL_TXT_WHITE);
    dwarf_set(112, 120, true);
    dwarf_play(won ? DWARF_DIG : DWARF_IDLE);
}

// Bonus rooms have no bank: a synthetic entry carries the layout seed.
static PuzzleHeader nugget_hdr = { FAM_NUGGET, 6, 1, 0 };
static uint8_t nugget_payload[4];

static void room_enter(const RoomSave *resume)
{
    const RunNode *node = run_current(&run);
    BankEntry e;
    if (node->family == FAM_NUGGET) {
        u32 s = run.seed ^ (0x9E3779B9u * (run.layer + 1));
        for (int i = 0; i < 4; i++) nugget_payload[i] = (uint8_t)(s >> (8 * i));
        e.hdr = &nugget_hdr;
        e.payload = nugget_payload;
        e.payload_len = 4;
    } else if (!bank_get(node->family, node->puzzle, &e) && !bank_get(FAM_DIG, 0, &e)) {
        map_enter_with(NULL, 0);
        return;
    }
    RoomContext ctx = {
        .depth = run.layer + 1, .max_depth = run.layers,
        .lives = run.lives, .ore = run.ore, .hints = run.hints,
        .stability = run_stability(node), .reward_ore = run_reward_ore(node),
        .icon = map_node_icon(node),
        .time_budget = run_time_budget(node->difficulty) * (100 + run.time_bonus_pct) / 100,
    };
    bool prop = !resume && run.props > 0 && node->family != FAM_NUGGET;
    if (prop) { run.props--; ctx.stability += 2; }
    if (!room_begin(&e, &ctx, resume)) { map_enter_with(NULL, 0); return; }
    if (prop) room_message(S(STR_PROP_USED), PAL_TXT_GOLD);
    run.room_in_progress = 1;
    if (!resume) save_run_commit(&run, NULL);
    screen = SCR_ROOM;
}

// Arriving on a node: camps rest the dwarf, everything else is a room.
static void arrive(void)
{
    const RunNode *node = run_current(&run);
    if (node->kind == NODE_CAMP) {
        run_room_cleared(&run, 0);
        save_run_commit(&run, NULL);
        shop_mode = SHOP_CAMP;                    // the merchant keeps the camp
        shop_enter(SHOP_CAMP, save_profile(), &run);
        screen = SCR_SHOP;
    } else {
        room_enter(NULL);
    }
}

static void start_run(void)
{
    bool avail[FAM_COUNT] = {0};
    for (int f = 0; f < FAM_COUNT; f++) avail[f] = bank_count(f) > 0;
    avail[FAM_NUGGET] = true;
    run_set_available_families(avail);
    Profile *p = save_profile();
    run_new(&run, debug_seed ? debug_seed : frames * 2654435761u + 12345u, length_cursor, p->recent, RECENT_MAX);
    run_apply_powers(&run, p->powers);
    shop_apply_gear(p, &run);
    p->runs_started++;
    save_profile_commit();
    room_enter(NULL);            // layer 0 is the entrance room
}

static void continue_run(void)
{
    RoomSave room;
    if (!save_run_load(&run, &room)) { title_enter(); return; }
    bool avail[FAM_COUNT] = {0};
    for (int f = 0; f < FAM_COUNT; f++) avail[f] = bank_count(f) > 0;
    avail[FAM_NUGGET] = true;
    run_set_available_families(avail);
    if (run.room_in_progress) room_enter(room.len ? &room : NULL);
    else map_enter_with(NULL, 0);
}

static void after_room(int outcome)
{
    const RoomResult *r = room_result();
    const RunNode *node = run_current(&run);
    run.hints = (u8)clampi(run.hints - r->hints_used, 0, 9);
    save_profile_add_recent(node->family, node->puzzle);
    if (outcome == ROOM_DONE) {
        run_room_cleared(&run, r->ore_gained);
        if (run_at_core(&run)) { run_end_enter(true); return; }
    } else {
        run_room_failed(&run);
        if (run_is_over(&run) || run_at_core(&run)) { run_end_enter(false); return; }
    }
    save_profile_commit();
    save_run_commit(&run, NULL);
    map_enter_with(NULL, 0);
}

int main(void)
{
    irq_init(NULL);
    irq_enable(II_VBLANK);
    bank_init();
    save_init();
    render_init();
    sound_init();
    sound_set_enabled(save_profile()->sound != 0);
    dwarf_cosmetics(save_profile()->cosmetics);
    lang_set(save_profile()->lang);
    lang_enter();

    for (;;) {
        vid_vsync();
        render_vblank();
        sound_update();
        input_poll();
        frames++;

        switch (screen) {
        case SCR_LANG:
            if (input_hit(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
                lang_cursor = (lang_cursor + 1) % LANG_COUNT;
                sfx_play(SFX_MOVE);
                lang_draw();
            }
            if (input_hit(KEY_A | KEY_START)) {
                lang_set(lang_cursor);
                if (save_profile()->lang != lang_cursor) {
                    save_profile()->lang = (u8)lang_cursor;
                    save_profile_commit();
                }
                sfx_play(SFX_MARK);
                title_enter();
            }
            break;
        case SCR_TITLE:
            if (input_hit(KEY_UP | KEY_DOWN)) {
                int dir = input_hit(KEY_UP) ? -1 : 1;
                do menu_cursor = (menu_cursor + dir + MENU_COUNT) % MENU_COUNT; while (!menu_enabled(menu_cursor));
                sfx_play(SFX_MOVE);
                title_draw_menu();
            }
            if (input_hit(KEY_SELECT)) {
                sound_set_enabled(!sound_enabled());
                save_profile()->sound = sound_enabled();
                save_profile_commit();
                title_draw_sound();
                sfx_play(SFX_MARK);
            }
            if (input_hit(KEY_START | KEY_A)) {
                if (menu_cursor == MENU_CONTINUE) continue_run();
                else if (menu_cursor == MENU_NEW) length_enter();
                else if (menu_cursor == MENU_SHOP) { shop_mode = SHOP_META; shop_enter(SHOP_META, save_profile(), NULL); screen = SCR_SHOP; }
                else records_enter();
            }
            break;
        case SCR_RECORDS:
            if (input_hit(KEY_B | KEY_START | KEY_A)) title_enter();
            break;
        case SCR_SHOP:
            if (shop_mode == SHOP_CAMP) run.frames++;
            if (shop_update() == SHOPSCREEN_LEAVE) {
                if (shop_mode == SHOP_META) {
                    if (shop_bought_something()) { save_profile_commit(); dwarf_cosmetics(save_profile()->cosmetics); }
                    title_enter();
                } else {
                    if (shop_bought_something()) save_run_commit(&run, NULL);
                    map_enter_with(S(STR_CAMP_REST), PAL_TXT_GOLD);
                }
            }
            break;
        case SCR_LENGTH:
            if (input_hit(KEY_UP | KEY_DOWN)) {
                int dir = input_hit(KEY_UP) ? -1 : 1;
                int open = save_profile()->lengths_unlocked;
                length_cursor = (length_cursor + dir + open) % open;
                sfx_play(SFX_MOVE);
                length_draw();
            }
            if (input_hit(KEY_B)) title_enter();
            else if (input_hit(KEY_A | KEY_START)) { sfx_play(SFX_MARK); start_run(); }
            break;
        case SCR_MAP:
            run.frames++;
            if (map_update(&run) == MAP_ARRIVED) arrive();
            break;
        case SCR_ROOM: {
            run.frames++;
            int outcome = room_update();
            if (outcome != ROOM_RUNNING) after_room(outcome);
            else if (room_take_dirty()) {
                RoomSave snap;
                room_snapshot(&snap);
                save_run_commit(&run, &snap);
            }
            break;
        }
        case SCR_RUN_END:
            if (input_hit(KEY_START | KEY_A)) title_enter();
            break;
        }
    }
}
