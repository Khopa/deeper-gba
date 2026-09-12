// Deeper — entry point and top-level screen flow.
#include "common.h"
#include "input.h"
#include "render.h"
#include "lang.h"
#include "bank.h"
#include "room.h"
#include "rng.h"

enum { SCR_TITLE, SCR_ROOM };

static int screen;
static RoomContext ctx = { .depth = 1, .max_depth = 30, .lives = 3, .ore = 0, .hints = 3 };
static u32 boot_frames;

static void title_enter(void)
{
    screen = SCR_TITLE;
    render_clear();
    txt_puts_center(6, S(STR_TITLE), PAL_TXT_GOLD);
    txt_puts_center(12, S(STR_PRESS_START), PAL_TXT_WHITE);
    dwarf_set(112, 64, true);
    dwarf_play(DWARF_IDLE);
}

// Difficulty window for a given depth: gentle start, rising with the descent.
static void difficulty_for_depth(int depth, int max_depth, int *lo, int *hi)
{
    int t = (depth - 1) * 9 / (max_depth - 1);      // 0..9
    *lo = clampi(1 + t - 1, 1, 10);
    *hi = clampi(1 + t + 1, 1, 10);
    if (depth <= 3) *lo = 1, *hi = 2;
}

static void room_enter(void)
{
    int lo, hi, first, last;
    difficulty_for_depth(ctx.depth, ctx.max_depth, &lo, &hi);
    bank_range(FAM_DIG, lo, hi, &first, &last);
    if (last <= first) bank_range(FAM_DIG, 1, 10, &first, &last);
    BankEntry e;
    bank_get(FAM_DIG, first + (int)rng_range((u32)(last - first)), &e);
    room_begin(&e, &ctx);
    screen = SCR_ROOM;
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
        boot_frames++;

        switch (screen) {
        case SCR_TITLE:
            if (input_hit(KEY_START | KEY_A)) {
                rng_seed(boot_frames * 2654435761u + 1);
                ctx.depth = 1;
                ctx.ore = 0;
                room_enter();
            }
            break;
        case SCR_ROOM:
            if (room_update() == ROOM_DONE) {
                const RoomResult *r = room_result();
                ctx.ore += r->ore_gained;
                if (ctx.depth >= ctx.max_depth) title_enter();
                else { ctx.depth++; room_enter(); }
            }
            break;
        }
    }
}
