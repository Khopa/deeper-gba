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
#include "fightscreen.h"
#include "mashscreen.h"

// SPLASH is the full-screen title picture (mode 4); TITLE is the menu behind it
enum { SCR_TITLE, SCR_RECORDS, SCR_MAP, SCR_ROOM, SCR_RUN_END, SCR_LANG, SCR_LENGTH, SCR_SHOP, SCR_SPLASH, SCR_OPTIONS, SCR_CRATES, SCR_FIGHT, SCR_MASH };
enum { MENU_CONTINUE, MENU_NEW, MENU_SHOP, MENU_RECORDS, MENU_OPTIONS, MENU_COUNT };
enum { OPT_SOUND, OPT_LANG, OPT_COUNT };

// Not static: the emulator scenarios (tests/emu) read them from RAM.
int screen;
RunState run;
u32 frames;
int menu_cursor;
int lang_cursor;
int length_cursor;              // descent length picked on the length screen
int options_cursor;
u32 debug_seed;                 // when non-zero, the next run uses it (emulator scenarios)
static bool new_length, new_record;
static int shop_mode;
static int logo_zoom;                  // the menu logo pops in from half size to three quarters
#define LOGO_ZOOM_START 128
#define LOGO_ZOOM_END   192

// --- title ----------------------------------------------------------------------------

static bool menu_enabled(int item) { return item != MENU_CONTINUE || save_has_run(); }

// Title menu: a row of buttons under the logo (the highlighted one lit and
// bobbing, the others dimmed), the highlighted entry named below the row.
// The art is 48 px in a 64 px sprite box: five of them fill the width.
#define MENU_ICON_X(i) (48 * (i) - 8)
#define MENU_ICON_Y    88
#define MENU_LABEL_ROW 19
static const int menu_labels[MENU_COUNT] = { STR_CONTINUE, STR_NEW_RUN, STR_SHOP, STR_RECORDS, STR_OPTIONS };
static const int menu_icons[MENU_COUNT] = { MICON_CONTINUE, MICON_NEW, MICON_SHOP, MICON_RECORDS, MICON_OPTIONS };

static void title_draw_menu(void)
{
    for (int i = 0; i < MENU_COUNT; i++)
        menu_icon_set(i, menu_icons[i], MENU_ICON_X(i), MENU_ICON_Y, menu_enabled(i) && i == menu_cursor, true);
    txt_clear_rect(0, MENU_LABEL_ROW, TILES_W, 1);
    txt_puts_center(MENU_LABEL_ROW, S(menu_labels[menu_cursor]), PAL_TXT_GOLD);
}

// A button icon followed by its label; labels in the string table start with the
// key letter and a space, which the icon replaces.
static void control_hint(int tx, int ty, int button, const char *label)
{
    button_icon(tx, ty, button);
    if (label[0] && label[1] == ' ') label += 2;
    txt_puts(tx + 3, ty, label, PAL_TXT_GRAY);
}

static void title_animate(void)
{
    if (logo_zoom < LOGO_ZOOM_END) {              // eases in over the first frames
        logo_zoom += (LOGO_ZOOM_END - logo_zoom) / 5 + 1;
        if (logo_zoom > LOGO_ZOOM_END) logo_zoom = LOGO_ZOOM_END;
        logo_set_scale(logo_zoom);
    }
    int bob = ((frames >> 4) & 1) ? -2 : 0;
    menu_icon_set(menu_cursor, menu_icons[menu_cursor], MENU_ICON_X(menu_cursor), MENU_ICON_Y + bob, true, true);
}

// Options: sound on/off and the language, saved in the profile as they change
static void options_draw(void)
{
    static const int labels[OPT_COUNT] = { STR_SOUND, STR_LANGUAGE };
    for (int i = 0; i < OPT_COUNT; i++) {
        int row = 8 + 2 * i;
        bool sel = i == options_cursor;
        txt_clear_rect(0, row, TILES_W, 1);
        txt_puts(6, row, S(labels[i]), sel ? PAL_TXT_GOLD : PAL_TXT_WHITE);
        const char *value = i == OPT_SOUND ? S(sound_enabled() ? STR_ON : STR_OFF) : lang_name(lang_get());
        txt_puts(18, row, value, sel ? PAL_TXT_GOLD : PAL_TXT_GRAY);
        if (sel) txt_puts(4, row, ">", PAL_TXT_GOLD);
    }
}

static void options_enter(void)
{
    screen = SCR_OPTIONS;
    render_clear();
    render_set_biome(BIOME_EARTH);
    options_cursor = 0;
    txt_puts_center(4, S(STR_OPTIONS), PAL_TXT_GOLD);
    options_draw();
    control_hint(1, 19, BTN_A, S(STR_NEXT));
    control_hint(12, 19, BTN_B, S(STR_BACK));
    dwarf_set(212, 128, true);
    dwarf_play(DWARF_IDLE);
}

static void options_toggle(void)
{
    Profile *p = save_profile();
    if (options_cursor == OPT_SOUND) {
        sound_set_enabled(!sound_enabled());
        p->sound = sound_enabled();
    } else {
        lang_set((lang_get() + 1) % LANG_COUNT);
        p->lang = (u8)lang_get();
        txt_clear_rect(0, 4, TILES_W, 1);
        txt_puts_center(4, S(STR_OPTIONS), PAL_TXT_GOLD);
        txt_clear_rect(0, 19, TILES_W, 1);
        control_hint(1, 19, BTN_A, S(STR_NEXT));
        control_hint(12, 19, BTN_B, S(STR_BACK));
    }
    save_profile_commit();
    sfx_play(SFX_MARK);
    options_draw();
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

// The title picture (mode 4). START or A goes on to the menu; render_init()
// rebuilds the tile layers the bitmap overwrote.
static void splash_enter(void)
{
    screen = SCR_SPLASH;
    render_title_picture();
    music_play(MUS_MAP);
}

static void dress_dwarf(void) { dwarf_cosmetics(save_profile()->upgrade[UPG_HELMET] >= 3 ? 1 : 0); }   // the golden helmet

static void splash_leave(void)
{
    render_init();
    dress_dwarf();
}

static void title_enter(void)
{
    screen = SCR_TITLE;
    render_clear();
    render_set_biome(BIOME_EARTH);
    music_play(MUS_MAP);
    logo_zoom = LOGO_ZOOM_START;
    logo_set_scale(logo_zoom);
    logo_show(true);
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
    icon_sprite(0, ICON_ORE, 17 * 8, 6 * 8 - 4, true);
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

    // the merchant's gear, level by level
    txt_puts(2, 13, S(STR_SHOP_TITLE), PAL_TXT_GOLD);
    for (int i = 0; i < SHOP_META_COUNT; i++) {
        int item = SHOP_META_FIRST + i;
        int level = shop_level(p, NULL, item), max = shop_item(item)->max_level;
        int tx = 2 + 6 * i;
        icon_at(tx, 14, shop_item_icon(p, item), level ? ICON_LIT : ICON_DIM, 1 + i);
        for (int k = 0; k < max; k++) txt_puts(tx + k, 16, "\x03", k < level ? PAL_TXT_GOLD : PAL_TXT_GRAY);
    }
    render_palettes_icons();
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
    save_profile_commit();
    save_run_clear();

    render_clear();
    music_play(won ? MUS_VICTORY : MUS_DEFEAT);
    sfx_play(won ? SFX_SOLVED : SFX_COLLAPSE);
    txt_puts_center(4, S(won ? STR_RUN_WON : STR_RUN_LOST), won ? PAL_TXT_GOLD : PAL_TXT_RED);
    txt_puts_center(7, S(STR_DEPTH), PAL_TXT_GRAY);
    put_number_center(8, run.layer + 1, PAL_TXT_WHITE);
    txt_puts(2, 10, S(STR_TOTAL_ORE), PAL_TXT_GRAY);
    icon_sprite(0, ICON_ORE, 2 * 8, 11 * 8 - 4, true);
    txt_putint(5, 11, run.ore, PAL_TXT_GOLD);
    txt_puts(20, 10, S(STR_TIME), PAL_TXT_GRAY);
    put_time(20, 11, run.frames, new_record ? PAL_TXT_GOLD : PAL_TXT_WHITE);
    int row = 13;
    if (new_record) txt_puts_center(row++, S(STR_NEW_RECORD), PAL_TXT_GOLD);
    if (new_length) txt_puts_center(row++, S(STR_LENGTH_UNLOCKED), PAL_TXT_GOLD);
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
        nugget_hdr.difficulty = node->difficulty;
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
    bool rope = run.rope_rooms > 0;               // the rope holds the next rooms, this one included
    if (rope) { if (!resume) run.rope_rooms--; ctx.stability += 2; }
    if (!room_begin(&e, &ctx, resume)) { map_enter_with(NULL, 0); return; }
    if (rope && !resume) room_message(S(STR_ROPE_USED), PAL_TXT_GOLD);
    run.room_in_progress = 1;
    if (!resume) save_run_commit(&run, NULL);
    screen = SCR_ROOM;
}

// --- the crates: three boxes, one empty, one small, one big -----------------------------
int crates_cursor;                     // not static: the emulator scenarios read it
static int crate_content[3];           // 0 empty, 1 small, 2 big (shuffled on arrival)
static bool crate_opened;
#define CRATE_TX(i) (7 + 7 * (i))
#define CRATE_TY    9

static void crates_draw(void)
{
    for (int i = 0; i < 3; i++) {
        icon_at(CRATE_TX(i), CRATE_TY, ICON_CRATE, i == crates_cursor && !crate_opened ? ICON_LIT : ICON_DIM, 1);
        txt_clear_rect(CRATE_TX(i) - 2, CRATE_TY + 2, 6, 1);
    }
    if (!crate_opened) cursor_set_px(CRATE_TX(crates_cursor) * 8, CRATE_TY * 8, true);
}

static void crates_enter(void)
{
    screen = SCR_CRATES;
    render_clear();
    render_palettes_icons();
    crates_cursor = 1;
    crate_opened = false;
    // a random order of the three contents
    crate_content[0] = 0; crate_content[1] = 1; crate_content[2] = 2;
    for (int i = 2; i > 0; i--) {
        int j = (int)rng_range((u32)(i + 1));
        int t = crate_content[i]; crate_content[i] = crate_content[j]; crate_content[j] = t;
    }
    txt_puts_center(2, S(STR_CRATES), PAL_TXT_GOLD);
    txt_puts_center(4, S(STR_CRATES_PICK), PAL_TXT_WHITE);
    crates_draw();
    dwarf_set(112, 128, true);
    dwarf_play(DWARF_IDLE);
}

static void crates_open(void)
{
    const RunNode *node = run_current(&run);
    crate_opened = true;
    int what = crate_content[crates_cursor], small = run_crate_ore(node);
    int ore = what == 0 ? 0 : what == 1 ? small : small * 3;
    crates_draw();
    cursor_set_px(0, 0, false);
    for (int i = 0; i < 3; i++) {                 // every crate shows what it held
        int w = crate_content[i], o = w == 0 ? 0 : w == 1 ? small : small * 3;
        if (o) icon_label(1 + i, ICON_ORE, CRATE_TX(i) - 1, CRATE_TY + 2, "+", o, i == crates_cursor ? PAL_TXT_GOLD : PAL_TXT_GRAY);
        else txt_puts(CRATE_TX(i) + 1, CRATE_TY + 2, "-", PAL_TXT_GRAY);
    }
    txt_puts_center(14, S(what == 0 ? STR_CRATE_EMPTY : what == 1 ? STR_CRATE_SMALL : STR_CRATE_BIG),
                    what == 0 ? PAL_TXT_GRAY : PAL_TXT_GOLD);
    sfx_play(what == 0 ? SFX_ERROR : SFX_COLLECT);
    run_room_cleared(&run, ore);
    save_run_commit(&run, NULL);
    dwarf_play(what ? DWARF_DIG : DWARF_IDLE);
    control_hint(11, 18, BTN_A, S(STR_NEXT));
}

// Arriving on a node: camps rest the dwarf, crates are opened, everything else is a room.
static void arrive(void)
{
    const RunNode *node = run_current(&run);
    if (node->kind == NODE_CRATES) { crates_enter(); return; }
    if (node->kind == NODE_FIGHT) { fight_enter(&run, node); screen = SCR_FIGHT; return; }
    if (node->kind == NODE_WALL) { mash_enter(&run, node); screen = SCR_MASH; return; }
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

// The helmet: a chance that a blow (cave-in, lost fight) costs no life
static bool helmet_holds(void)
{
    return run.helmet_pct > 0 && (int)rng_range(100) < run.helmet_pct;
}

static void after_room(int outcome)
{
    const RoomResult *r = room_result();
    const RunNode *node = run_current(&run);
    if (outcome == ROOM_QUIT) {                   // keep the room as it is and leave
        RoomSave snap;
        room_snapshot(&snap);
        save_run_commit(&run, &snap);
        title_enter();
        return;
    }
    run.hints = (u8)clampi(run.hints - r->hints_used, 0, 9);
    save_profile_add_recent(node->family, node->puzzle);
    if (outcome == ROOM_DONE) {
        run_room_cleared(&run, r->ore_gained);
        if (run_at_core(&run)) { run_end_enter(true); return; }
    } else {
        // a cave-in: the helmet may take the blow (giving up is a choice, no helmet)
        bool held = outcome == ROOM_COLLAPSED && helmet_holds();
        run_room_failed(&run, held);
        if (run_is_over(&run)) { run_end_enter(false); return; }
        if (run_at_core(&run)) {                  // the heart holds: try again while lives last
            run_reroll_core(&run);
            save_profile_commit();
            room_enter(NULL);
            room_message(S(held ? STR_HELMET_HELD : STR_CORE_AGAIN), held ? PAL_TXT_GOLD : PAL_TXT_RED);
            return;
        }
        if (held) {
            save_profile_commit();
            save_run_commit(&run, NULL);
            map_enter_with(S(STR_HELMET_HELD), PAL_TXT_GOLD);
            return;
        }
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
    dress_dwarf();
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
                splash_enter();
            }
            break;
        case SCR_SPLASH:
            title_prompt(S(STR_PRESS_START), 146, ((frames >> 5) & 1) == 0);
            if (input_hit(KEY_START | KEY_A)) {
                sfx_play(SFX_MARK);
                splash_leave();
                title_enter();
            }
            break;
        case SCR_TITLE:
            title_animate();
            if (input_hit(KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
                int dir = input_hit(KEY_UP | KEY_LEFT) ? -1 : 1;
                do menu_cursor = (menu_cursor + dir + MENU_COUNT) % MENU_COUNT; while (!menu_enabled(menu_cursor));
                sfx_play(SFX_MOVE);
                title_draw_menu();
            }
            if (input_hit(KEY_START | KEY_A)) {
                if (menu_cursor == MENU_CONTINUE) continue_run();
                else if (menu_cursor == MENU_NEW) length_enter();
                else if (menu_cursor == MENU_SHOP) { shop_mode = SHOP_META; shop_enter(SHOP_META, save_profile(), NULL); screen = SCR_SHOP; }
                else if (menu_cursor == MENU_RECORDS) records_enter();
                else options_enter();
            }
            break;
        case SCR_OPTIONS:
            if (input_hit(KEY_UP | KEY_DOWN)) {
                options_cursor = (options_cursor + 1) % OPT_COUNT;
                sfx_play(SFX_MOVE);
                options_draw();
            }
            if (input_hit(KEY_A | KEY_LEFT | KEY_RIGHT)) options_toggle();
            if (input_hit(KEY_B | KEY_START)) title_enter();
            break;
        case SCR_RECORDS:
            if (input_hit(KEY_B | KEY_START | KEY_A)) title_enter();
            break;
        case SCR_SHOP:
            if (shop_mode == SHOP_CAMP) run.frames++;
            if (shop_update() == SHOPSCREEN_LEAVE) {
                if (shop_mode == SHOP_META) {
                    if (shop_bought_something()) { save_profile_commit(); dress_dwarf(); }
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
        case SCR_MASH: {
            run.frames++;
            int outcome = mash_update();
            if (outcome != MASH_RUNNING) {
                run_room_cleared(&run, outcome == MASH_WON ? run_reward_ore(run_current(&run)) : 0);
                save_run_commit(&run, NULL);
                map_enter_with(NULL, 0);
            }
            break;
        }
        case SCR_FIGHT: {
            run.frames++;
            int outcome = fight_update();
            if (outcome == FIGHT_WON) {
                run_room_cleared(&run, run_reward_ore(run_current(&run)));
                save_run_commit(&run, NULL);
                map_enter_with(NULL, 0);
            } else if (outcome == FIGHT_LOST) {
                bool held = helmet_holds();
                run_room_failed(&run, held);
                if (run_is_over(&run)) { run_end_enter(false); break; }
                save_run_commit(&run, NULL);
                map_enter_with(S(held ? STR_HELMET_HELD : STR_FIGHT_LOST), held ? PAL_TXT_GOLD : PAL_TXT_RED);
            }
            break;
        }
        case SCR_CRATES:
            run.frames++;
            if (!crate_opened) {
                if (input_hit(KEY_LEFT | KEY_RIGHT)) {
                    crates_cursor = (crates_cursor + (input_hit(KEY_LEFT) ? 2 : 1)) % 3;
                    sfx_play(SFX_MOVE);
                    crates_draw();
                }
                if (input_hit(KEY_A)) crates_open();
            } else if (input_hit(KEY_A | KEY_START)) {
                map_enter_with(NULL, 0);
            }
            break;
        case SCR_MAP:
            run.frames++;
            if (map_update(&run) == MAP_ARRIVED) arrive();
            break;
        case SCR_ROOM: {
            if (!room_paused()) run.frames++;
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
