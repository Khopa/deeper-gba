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

enum { SCR_TITLE, SCR_RECORDS, SCR_MAP, SCR_ROOM, SCR_RUN_END };
enum { MENU_CONTINUE, MENU_NEW, MENU_RECORDS, MENU_COUNT };

// Not static: the emulator scenarios (tests/emu) read them from RAM.
int screen;
RunState run;
u32 frames;
int menu_cursor;
static u32 new_powers;

// --- title ----------------------------------------------------------------------------

static bool menu_enabled(int item) { return item != MENU_CONTINUE || save_has_run(); }

static void title_draw_menu(void)
{
    static const int labels[MENU_COUNT] = { STR_CONTINUE, STR_NEW_RUN, STR_RECORDS };
    for (int i = 0; i < MENU_COUNT; i++) {
        int pal = !menu_enabled(i) ? PAL_TXT_GRAY : i == menu_cursor ? PAL_TXT_GOLD : PAL_TXT_WHITE;
        txt_clear_rect(0, 11 + 2 * i, TILES_W, 1);
        txt_puts_center(11 + 2 * i, S(labels[i]), pal);
        if (i == menu_cursor) txt_puts((TILES_W - txt_len(S(labels[i]))) / 2 - 2, 11 + 2 * i, ">", PAL_TXT_GOLD);
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

static void title_enter(void)
{
    screen = SCR_TITLE;
    render_clear();
    render_set_biome(biome_info(BIOME_EARTH)->backdrop, biome_info(BIOME_EARTH)->accent);
    music_play(MUS_NONE);
    txt_puts_center(4, S(STR_TITLE), PAL_TXT_GOLD);
    title_draw_sound();
    dwarf_set(112, 52, true);
    dwarf_play(DWARF_IDLE);
    menu_cursor = save_has_run() ? MENU_CONTINUE : MENU_NEW;
    title_draw_menu();
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

    txt_puts(2, 9, S(STR_POWERS), PAL_TXT_GOLD);
    static const int names[POWER_COUNT] = { STR_POWER_LAMP, STR_POWER_TOUGH, STR_POWER_SECOND };
    static const int howto[POWER_COUNT] = { STR_UNLOCK_LAMP, STR_UNLOCK_TOUGH, STR_UNLOCK_SECOND };
    for (int i = 0; i < POWER_COUNT; i++) {
        bool owned = (p->powers >> i) & 1;
        txt_puts(2, 11 + 2 * i, owned ? "*" : ".", owned ? PAL_TXT_GOLD : PAL_TXT_GRAY);
        txt_puts(4, 11 + 2 * i, S(names[i]), owned ? PAL_TXT_WHITE : PAL_TXT_GRAY);
        if (!owned) txt_puts(4, 12 + 2 * i, S(howto[i]), PAL_TXT_GRAY);
    }
    txt_puts_center(18, S(STR_BACK), PAL_TXT_GRAY);
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
    txt_puts_center(10, S(STR_TOTAL_ORE), PAL_TXT_GRAY);
    put_number_center(11, run.ore, PAL_TXT_GOLD);
    if (new_powers) {
        txt_puts_center(13, S(STR_NEW_POWER), PAL_TXT_GOLD);
        static const int names[POWER_COUNT] = { STR_POWER_LAMP, STR_POWER_TOUGH, STR_POWER_SECOND };
        int row = 14;
        for (int i = 0; i < POWER_COUNT; i++)
            if ((new_powers >> i) & 1) txt_puts_center(row++, S(names[i]), PAL_TXT_WHITE);
    }
    txt_puts_center(17, S(STR_PRESS_START), PAL_TXT_WHITE);
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
        .depth = run.layer + 1, .max_depth = RUN_LAYERS,
        .lives = run.lives, .ore = run.ore, .hints = run.hints,
        .stability = run_stability(node), .reward_ore = run_reward_ore(node),
        .icon = map_node_icon(node),
    };
    if (!room_begin(&e, &ctx, resume)) { map_enter_with(NULL, 0); return; }
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
        map_enter_with(S(STR_CAMP_REST), PAL_TXT_GOLD);
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
    run_new(&run, frames * 2654435761u + 12345u, p->recent, RECENT_MAX);
    run_apply_powers(&run, p->powers);
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
    lang_set(save_profile()->lang);
    title_enter();

    for (;;) {
        vid_vsync();
        render_vblank();
        sound_update();
        input_poll();
        frames++;

        switch (screen) {
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
                else if (menu_cursor == MENU_NEW) start_run();
                else records_enter();
            }
            break;
        case SCR_RECORDS:
            if (input_hit(KEY_B | KEY_START | KEY_A)) title_enter();
            break;
        case SCR_MAP:
            if (map_update(&run) == MAP_ARRIVED) arrive();
            break;
        case SCR_ROOM: {
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
