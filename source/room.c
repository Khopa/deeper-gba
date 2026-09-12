#include "room.h"
#include "puzzle.h"
#include "render.h"
#include "input.h"
#include "lang.h"

// Layout (tiles): grid area is 16x16 tiles from (1,3); smaller grids are
// centred inside it. The right panel starts at tile 18.
#define GRID_AREA_TX 1
#define GRID_AREA_TY 3
#define PANEL_TX     18
#define DWARF_X      196
#define DWARF_Y      28

#define SOLVED_FRAMES   90      // celebration before "A continue" is accepted
#define COLLAPSE_FRAMES 120
#define MSG_FRAMES      120     // transient message duration

enum { ST_PLAY, ST_ASK_ABANDON, ST_SOLVED, ST_COLLAPSE, ST_DONE };

static const PuzzleOps *ops;
static RoomContext ctx;
static RoomResult result;
static int state, timer, msg_timer, outcome;
static int cur_r, cur_c, n;
static int hints_left, stability;

// --- drawing ---------------------------------------------------------------------

static void draw_grid(void)
{
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            CellView v;
            ops->cell(r, c, &v);
            grid_cell(r, c, v.edges, v.pal);
            grid_mark(r, c, v.mark);
        }
}

static void draw_panel(void)
{
    int x = PANEL_TX;
    txt_clear_rect(x, 5, TILES_W - x, 15);
    txt_puts(x, 5, S(STR_LIVES), PAL_TXT_GRAY);
    for (int i = 0; i < 5; i++)
        txt_puts(x + i, 6, i < ctx.lives ? "\x04" : "\x05", PAL_TXT_RED);
    txt_puts(x, 8, S(STR_STABILITY), PAL_TXT_GRAY);
    for (int i = 0; i < ctx.stability; i++)
        txt_puts(x + i, 9, "\x03", i < stability ? PAL_TXT_GOLD : PAL_TXT_GRAY);
    txt_puts(x, 11, S(STR_ORE), PAL_TXT_GRAY);
    txt_putint(x, 12, ctx.ore, PAL_TXT_GOLD);
    txt_puts(x, 14, S(STR_HINTS), PAL_TXT_GRAY);
    txt_putint(x, 15, hints_left, PAL_TXT_WHITE);
    txt_puts(x, 17, S(ops->key_a_str), PAL_TXT_GRAY);
    txt_puts(x, 18, S(ops->key_b_str), PAL_TXT_GRAY);
    txt_puts(x, 19, S(STR_KEY_L_HINT), PAL_TXT_GRAY);
}

static void draw_header(void)
{
    txt_clear_rect(0, 0, TILES_W, 2);
    txt_puts(1, 0, S(ops->name_str), PAL_TXT_GOLD);
    int x = TILES_W - 1 - txt_len(S(STR_DEPTH)) - 6;
    txt_puts(x, 0, S(STR_DEPTH), PAL_TXT_GRAY);
    txt_putint(x + txt_len(S(STR_DEPTH)) + 1, 0, ctx.depth, PAL_TXT_WHITE);
    txt_puts(x + txt_len(S(STR_DEPTH)) + 3, 0, "/", PAL_TXT_GRAY);
    txt_putint(x + txt_len(S(STR_DEPTH)) + 4, 0, ctx.max_depth, PAL_TXT_WHITE);
}

static void show_message(const char *s, int pal)
{
    txt_clear_rect(0, 1, TILES_W, 1);
    txt_puts_center(1, s, pal);
    msg_timer = MSG_FRAMES;
}

// --- flow -------------------------------------------------------------------------

bool room_begin(const BankEntry *e, const RoomContext *c)
{
    ops = puzzle_ops(e->hdr->family);
    if (!ops || !ops->load(e->hdr, e->payload)) return false;
    ctx = *c;
    memset(&result, 0, sizeof result);
    n = ops->size();
    cur_r = cur_c = 0;
    hints_left = ctx.hints;
    stability = ctx.stability;
    state = ST_PLAY;
    timer = msg_timer = 0;
    outcome = ROOM_RUNNING;

    render_clear();
    render_palettes_room();
    int off = (PUZZLE_MAX_N - n);          // centre smaller grids (tiles)
    grid_set_origin(GRID_AREA_TX + off, GRID_AREA_TY + off);
    draw_header();
    draw_grid();
    draw_panel();
    dwarf_set(DWARF_X, DWARF_Y, true);
    dwarf_play(DWARF_IDLE);
    cursor_set_cell(cur_r, cur_c, true);
    return true;
}

static int ore_reward(void)
{
    int penalty = result.hints_used * 5 + result.mistakes * 2;
    int r = ctx.reward_ore - penalty;
    int floor_ = ctx.reward_ore / 4;
    return r < floor_ ? floor_ : r;
}

static void on_solved(void)
{
    state = ST_SOLVED;
    timer = 0;
    result.solved = true;
    result.ore_gained = ore_reward();
    cursor_set_cell(0, 0, false);
    dwarf_play(DWARF_DIG);
    txt_clear_rect(0, 1, TILES_W, 1);
    txt_puts_center(1, S(STR_SOLVED), PAL_TXT_GOLD);
    txt_clear_rect(PANEL_TX, 17, TILES_W - PANEL_TX, 3);
    txt_puts(PANEL_TX, 17, S(STR_ORE_FOUND), PAL_TXT_GRAY);
    txt_puts(PANEL_TX, 18, "+", PAL_TXT_GOLD);
    txt_putint(PANEL_TX + 1, 18, result.ore_gained, PAL_TXT_GOLD);
}

static void on_collapse(void)
{
    state = ST_COLLAPSE;
    timer = 0;
    cursor_set_cell(0, 0, false);
    txt_clear_rect(0, 1, TILES_W, 1);
    txt_puts_center(1, S(STR_COLLAPSE), PAL_TXT_RED);
    txt_clear_rect(PANEL_TX, 17, TILES_W - PANEL_TX, 3);
}

static void play_update(void)
{
    int dr = 0, dc = 0;
    if (input_nav(KEY_UP))    dr = -1;
    if (input_nav(KEY_DOWN))  dr = 1;
    if (input_nav(KEY_LEFT))  dc = -1;
    if (input_nav(KEY_RIGHT)) dc = 1;
    if (dr || dc) {
        cur_r = (cur_r + dr + n) % n;
        cur_c = (cur_c + dc + n) % n;
        cursor_set_cell(cur_r, cur_c, true);
    }

    if (input_hit(KEY_A | KEY_B)) {
        ActionResult ar = ops->action(cur_r, cur_c, input_hit(KEY_A) ? ACT_A : ACT_B);
        if (ar.changed) {
            draw_grid();
            if (ar.mistake) {
                result.mistakes++;
                if (--stability <= 0) { on_collapse(); return; }
                draw_panel();
            }
            if (ar.solved) { on_solved(); return; }
        }
    }

    if (input_hit(KEY_L | KEY_R)) {
        if (hints_left <= 0) {
            show_message(S(STR_NO_HINTS), PAL_TXT_RED);
        } else {
            int r, c;
            int h = ops->hint(&r, &c);
            if (h == HINT_WRONG_PLACEMENT) {
                cur_r = r;
                cur_c = c;
                cursor_set_cell(cur_r, cur_c, true);
                show_message(S(STR_WRONG_DIG), PAL_TXT_RED);
            } else if (h == HINT_APPLIED) {
                hints_left--;
                result.hints_used++;
                cur_r = r;
                cur_c = c;
                cursor_set_cell(cur_r, cur_c, true);
                draw_grid();
                draw_panel();
                if (ops->solved()) { on_solved(); return; }
            }
        }
    }

    if (input_hit(KEY_SELECT)) {
        state = ST_ASK_ABANDON;
        txt_clear_rect(0, 1, TILES_W, 1);
        txt_puts_center(1, S(STR_ABANDON_ASK), PAL_TXT_RED);
        txt_clear_rect(PANEL_TX, 17, TILES_W - PANEL_TX, 3);
        txt_puts(PANEL_TX, 18, S(STR_YES_NO), PAL_TXT_WHITE);
        return;
    }

    if (msg_timer && --msg_timer == 0) txt_clear_rect(0, 1, TILES_W, 1);
}

int room_update(void)
{
    switch (state) {
    case ST_PLAY:
        play_update();
        return ROOM_RUNNING;
    case ST_ASK_ABANDON:
        if (input_hit(KEY_A)) { state = ST_DONE; outcome = ROOM_ABANDONED; return outcome; }
        if (input_hit(KEY_B | KEY_SELECT)) {
            state = ST_PLAY;
            txt_clear_rect(0, 1, TILES_W, 1);
            draw_panel();
        }
        return ROOM_RUNNING;
    case ST_SOLVED:
        if (++timer == SOLVED_FRAMES) {
            dwarf_play(DWARF_IDLE);
            txt_puts(PANEL_TX, 19, S(STR_NEXT), PAL_TXT_WHITE);
        }
        if (timer >= SOLVED_FRAMES && input_hit(KEY_A | KEY_START)) { state = ST_DONE; outcome = ROOM_DONE; }
        return outcome;
    case ST_COLLAPSE:
        if (++timer >= COLLAPSE_FRAMES && input_hit(KEY_A | KEY_START)) { state = ST_DONE; outcome = ROOM_COLLAPSED; }
        if (timer == COLLAPSE_FRAMES) txt_puts(PANEL_TX, 19, S(STR_NEXT), PAL_TXT_WHITE);
        return outcome;
    default:
        return outcome;
    }
}

const RoomResult *room_result(void) { return &result; }
