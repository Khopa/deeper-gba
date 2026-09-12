// Deeper — entry point and top-level screen flow:
//   title -> map <-> room ... -> run end (core reached / no lives left) -> title
#include "common.h"
#include "input.h"
#include "render.h"
#include "lang.h"
#include "bank.h"
#include "room.h"
#include "run.h"
#include "mapscreen.h"
#include "rng.h"

enum { SCR_TITLE, SCR_MAP, SCR_ROOM, SCR_RUN_END };

static int screen;
static RunState run;
static u32 frames;
static bool run_won;

static void title_enter(void)
{
    screen = SCR_TITLE;
    render_clear();
    txt_puts_center(6, S(STR_TITLE), PAL_TXT_GOLD);
    txt_puts_center(12, S(STR_PRESS_START), PAL_TXT_WHITE);
    dwarf_set(112, 64, true);
    dwarf_play(DWARF_IDLE);
}

static void map_enter_with(const char *msg, int pal)
{
    map_enter(&run);
    if (msg) txt_puts_center(18, msg, pal);
    else txt_puts_center(18, S(STR_MAP_HELP), PAL_TXT_GRAY);
    screen = SCR_MAP;
}

static void run_end_enter(bool won)
{
    screen = SCR_RUN_END;
    run_won = won;
    render_clear();
    txt_puts_center(5, S(won ? STR_RUN_WON : STR_RUN_LOST), won ? PAL_TXT_GOLD : PAL_TXT_RED);
    txt_puts_center(9, S(STR_TOTAL_ORE), PAL_TXT_GRAY);
    char buf[8];
    int v = run.ore, len = 0;
    if (v >= 1000) buf[len++] = (char)('0' + v / 1000);
    if (v >= 100) buf[len++] = (char)('0' + (v / 100) % 10);
    if (v >= 10) buf[len++] = (char)('0' + (v / 10) % 10);
    buf[len++] = (char)('0' + v % 10);
    buf[len] = 0;
    txt_puts_center(10, buf, PAL_TXT_GOLD);
    txt_puts_center(15, S(STR_PRESS_START), PAL_TXT_WHITE);
    dwarf_set(112, 96, true);
    dwarf_play(won ? DWARF_DIG : DWARF_IDLE);
}

static void room_enter(void)
{
    const RunNode *node = run_current(&run);
    BankEntry e;
    if (!bank_get(node->family, node->puzzle, &e) && !bank_get(FAM_DIG, 0, &e)) {
        map_enter_with(NULL, 0);
        return;
    }
    RoomContext ctx = {
        .depth = run.layer + 1, .max_depth = RUN_LAYERS,
        .lives = run.lives, .ore = run.ore, .hints = run.hints,
        .stability = run_stability(node), .reward_ore = run_reward_ore(node),
        .icon = map_node_icon(node),
    };
    if (!room_begin(&e, &ctx)) { map_enter_with(NULL, 0); return; }
    screen = SCR_ROOM;
}

// Arriving on a node: camps rest the dwarf, everything else is a room.
static void arrive(void)
{
    const RunNode *node = run_current(&run);
    if (node->kind == NODE_CAMP) {
        run_room_cleared(&run, 0);
        map_enter_with(S(STR_CAMP_REST), PAL_TXT_GOLD);
    } else {
        room_enter();
    }
}

static void start_run(void)
{
    bool avail[FAM_COUNT] = {0};
    for (int f = 0; f < FAM_COUNT; f++) avail[f] = bank_count(f) > 0;
    run_set_available_families(avail);
    run_new(&run, frames * 2654435761u + 12345u, NULL, 0);
    // layer 0 is the entrance room: play it right away
    room_enter();
}

static void after_room(int outcome)
{
    const RoomResult *r = room_result();
    if (outcome == ROOM_DONE) {
        run.hints -= r->hints_used;
        run_room_cleared(&run, r->ore_gained);
        if (run_at_core(&run)) { run_end_enter(true); return; }
    } else {
        run.hints -= r->hints_used;
        run_room_failed(&run);
        if (run_is_over(&run)) { run_end_enter(false); return; }
        if (run_at_core(&run)) { run_end_enter(false); return; }
    }
    map_enter_with(NULL, 0);
}

int main(void)
{
    irq_init(NULL);
    irq_enable(II_VBLANK);
    bank_init();
    render_init();
    lang_set(LANG_FR);
    title_enter();

    for (;;) {
        vid_vsync();
        render_vblank();
        input_poll();
        frames++;

        switch (screen) {
        case SCR_TITLE:
            if (input_hit(KEY_START | KEY_A)) start_run();
            break;
        case SCR_MAP:
            if (map_update(&run) == MAP_ARRIVED) arrive();
            break;
        case SCR_ROOM: {
            int outcome = room_update();
            if (outcome != ROOM_RUNNING) after_room(outcome);
            break;
        }
        case SCR_RUN_END:
            if (input_hit(KEY_START | KEY_A)) title_enter();
            break;
        }
    }
}
