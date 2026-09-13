#include "room.h"
#include "puzzle.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "sound.h"
#include "biome.h"

// Layout (tiles): grid area is 16x16 tiles from (1,3); smaller grids are
// centred inside it. The right panel starts at tile 18.
#define GRID_AREA_TX 1
#define GRID_AREA_TY 3
#define PANEL_TX     18
#define DWARF_X      196
#define DWARF_Y      28

#define SOLVED_FRAMES   90      // celebration before "A continue" is accepted
#define MISTAKE_DELAY   120     // frames a conflict may stand before it costs stability
#define WARN_FROM       60      // ...and from when it starts blinking
#define BURST_FRAMES    18      // explosion effect length
// Time bar: 128 px under the grid area (rows 19), drains over the room budget
#define TIMEBAR_X  8
#define TIMEBAR_Y  153
#define TIMEBAR_W  128
#define TIMEBAR_H  6
#define COLLAPSE_FRAMES 120
#define MSG_FRAMES      120     // transient message duration

enum { ST_PLAY, ST_ASK_ABANDON, ST_SOLVED, ST_COLLAPSE, ST_DONE };

static const PuzzleOps *ops;
static RoomContext ctx;
static RoomResult result;
static int state, timer, msg_timer, outcome;
int room_cur_r, room_cur_c;            // the cursor; not static: read by the emulator scenarios
static int n;
static int hints_left, stability;
static int elapsed;                          // frames spent in play
static bool dirty;
static u8 conflict_age[PUZZLE_MAX_CELLS];   // frames each cell has been in conflict (255 = already charged)
#define MAX_BURSTS 4
static int burst_cell[MAX_BURSTS], burst_timer[MAX_BURSTS];   // explosion effects
static u8 ghost_cells[2 * 8];
static int ghost_count;
static bool ghost_fits;

static void on_solved(void);
static void on_collapse(void);
static void draw_panel(void);

// --- drawing ---------------------------------------------------------------------

// Ghost preview of the current block: outline marks over the cells it would cover
static void draw_ghost(void)
{
    if (!ops->ghost) return;
    for (int i = 0; i < ghost_count; i++) {          // erase the previous ghost
        int r = ghost_cells[2 * i], c = ghost_cells[2 * i + 1];
        CellView v;
        ops->cell(r, c, &v);
        grid_mark(r, c, v.mark);
    }
    ghost_count = ops->ghost(room_cur_r, room_cur_c, ghost_cells, &ghost_fits);
    for (int i = 0; i < ghost_count; i++) {
        int r = ghost_cells[2 * i], c = ghost_cells[2 * i + 1];
        CellView v;
        ops->cell(r, c, &v);
        if (v.mark == MARK_NONE) grid_mark(r, c, ghost_fits ? MARK_GHOST_OK : MARK_GHOST_BAD);
    }
}

static void draw_grid(void)
{
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            CellView v;
            ops->cell(r, c, &v);
            grid_cell(r, c, v.variant * 16 + v.edges, v.pal);
            grid_mark(r, c, v.mark);
        }
    ghost_count = 0;
    draw_ghost();
}

// The time bar: what is left of the budget in the accent colour, the rest dim
static void draw_timebar(void)
{
    if (!ctx.time_budget) return;
    int left = ctx.time_budget - elapsed;
    if (left < 0) left = 0;
    int lit = TIMEBAR_W * left / ctx.time_budget;
    for (int y = 0; y < TIMEBAR_H; y++)
        for (int x = 0; x < TIMEBAR_W; x++)
            canvas_plot(TIMEBAR_X + x, TIMEBAR_Y + y, x < lit ? CANVAS_LINE_LIT : CANVAS_LINE);
}

static void start_burst(int cell)
{
    int slot = 0;
    for (int i = 0; i < MAX_BURSTS; i++) if (burst_timer[i] <= 0) { slot = i; break; }
    burst_cell[slot] = cell;
    burst_timer[slot] = BURST_FRAMES;
    sfx_play(SFX_ERROR);
}

// A mistake was confirmed: spend stability, cave in when it runs out.
// Returns false when the room ended.
static bool spend_stability(void)
{
    result.mistakes++;
    if (--stability <= 0) {
        if (ops->bonus_ore) on_solved(); else on_collapse();
        return false;
    }
    draw_panel();
    return true;
}

// Conflicts that stay on the board: blink as a warning, then cost stability
// once (the cell keeps its red colour until the player fixes it).
static void watch_conflicts(void)
{
    if (ops->immediate_mistakes) return;
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            int i = cell_at(n, r, c);
            CellView v;
            ops->cell(r, c, &v);
            if (!v.conflict) { conflict_age[i] = 0; continue; }
            if (conflict_age[i] == 255) continue;
            conflict_age[i]++;
            if (conflict_age[i] >= WARN_FROM && conflict_age[i] < MISTAKE_DELAY)
                grid_cell_pal(r, c, ((conflict_age[i] >> 3) & 1) ? PAL_CELL_HILITE : PAL_CELL_CONFLICT);
            if (conflict_age[i] >= MISTAKE_DELAY) {
                conflict_age[i] = 255;
                grid_cell_pal(r, c, PAL_CELL_CONFLICT);
                start_burst(i);
                if (!spend_stability()) return;
            }
        }
}

static void update_burst(void)
{
    for (int i = 0; i < MAX_BURSTS; i++) {
        if (burst_timer[i] <= 0) continue;
        int r = burst_cell[i] / n, c = burst_cell[i] % n;
        if (--burst_timer[i] <= 0) {
            CellView v;
            ops->cell(r, c, &v);
            grid_mark(r, c, v.mark);
            continue;
        }
        int phase = burst_timer[i] > 12 ? MARK_BURST1 : burst_timer[i] > 6 ? MARK_BURST2 : MARK_BURST3;
        grid_mark(r, c, phase);
    }
}

// Tray: the current block of a BLOCK room, drawn with solid glyphs in the
// panel's lower right corner (5x5 tiles from (25, 8)).
#define TRAY_TX 25
#define TRAY_TY 11
static void draw_tray(void)
{
    if (!ops->tray) return;
    uint8_t rc[2 * 8];
    int cells = ops->tray(rc, (int)sizeof rc);
    txt_clear_rect(TRAY_TX, TRAY_TY, 5, 5);
    for (int i = 0; i < cells; i++)
        txt_puts(TRAY_TX + rc[2 * i + 1], TRAY_TY + rc[2 * i], "\x06", PAL_TXT_GOLD);
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
    if (ops->aux) txt_puts(x, 16, S(ops->key_r_str), PAL_TXT_GRAY);
    draw_tray();
}

static void draw_header(void)
{
    txt_clear_rect(0, 0, TILES_W, 2);
    txt_puts(1, 0, S(ops->name_str), PAL_TXT_GOLD);
    const BiomeInfo *bi = biome_info(biome_for_layer(ctx.depth - 1, ctx.max_depth));
    txt_puts(2 + txt_len(S(ops->name_str)), 0, S(bi->name_str), PAL_TXT_GRAY);
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

bool room_begin(const BankEntry *e, const RoomContext *c, const RoomSave *resume)
{
    ops = puzzle_ops(e->hdr->family);
    if (!ops || !ops->load(e->hdr, e->payload)) return false;
    ctx = *c;
    memset(&result, 0, sizeof result);
    n = ops->size();
    room_cur_r = room_cur_c = 0;
    hints_left = ctx.hints;
    stability = ctx.stability;
    state = ST_PLAY;
    timer = msg_timer = 0;
    outcome = ROOM_RUNNING;
    dirty = false;
    elapsed = 0;
    if (ops->bonus_ore) ctx.time_budget = 0;      // bonus rooms are paced by their chances
    memset(conflict_age, 0, sizeof conflict_age);
    memset(burst_timer, 0, sizeof burst_timer);
    ghost_count = 0;
    if (resume && resume->len && ops->restore(resume->data, resume->len)) {
        stability = resume->stability;
        hints_left = resume->hints_left;
        result.mistakes = resume->mistakes;
        result.hints_used = resume->hints_used;
        room_cur_r = clampi(resume->cur_r, 0, n - 1);
        room_cur_c = clampi(resume->cur_c, 0, n - 1);
        elapsed = resume->elapsed;
    }

    render_clear();
    render_palettes_room();
    const BiomeInfo *bi = biome_info(biome_for_layer(ctx.depth - 1, ctx.max_depth));
    render_set_biome(bi->backdrop, bi->accent);
    music_play(bi->music);
    int off = (PUZZLE_MAX_N - n);          // centre smaller grids (tiles)
    grid_set_origin(GRID_AREA_TX + off, GRID_AREA_TY + off);
    draw_header();
    draw_grid();
    draw_panel();
    canvas_clear();
    draw_timebar();
    canvas_show(true);
    dwarf_set(DWARF_X, DWARF_Y, true);
    dwarf_play(DWARF_IDLE);
    cursor_set_cell(room_cur_r, room_cur_c, true);
    return true;
}

// Base reward minus hint/mistake penalties (never below a quarter), plus a
// speed bonus worth up to the base again when the time bar is still full.
static int ore_reward(void)
{
    if (ops->bonus_ore) return ops->bonus_ore();
    int penalty = result.hints_used * 5 + result.mistakes * 2;
    int r = ctx.reward_ore - penalty;
    int floor_ = ctx.reward_ore / 4;
    if (r < floor_) r = floor_;
    if (ctx.time_budget) {
        int left = ctx.time_budget - elapsed;
        if (left > 0) result.speed_bonus = ctx.reward_ore * left / ctx.time_budget;
    }
    return r + result.speed_bonus;
}

static void on_solved(void)
{
    state = ST_SOLVED;
    timer = 0;
    result.solved = true;
    result.frames = elapsed;
    result.ore_gained = ore_reward();
    cursor_set_cell(0, 0, false);
    dwarf_play(DWARF_DIG);
    sfx_play(SFX_SOLVED);
    txt_clear_rect(0, 1, TILES_W, 1);
    txt_puts_center(1, S(STR_SOLVED), PAL_TXT_GOLD);
    txt_clear_rect(PANEL_TX, 17, TILES_W - PANEL_TX, 3);
    txt_puts(PANEL_TX, 17, S(STR_ORE_FOUND), PAL_TXT_GRAY);
    txt_puts(PANEL_TX, 18, "+", PAL_TXT_GOLD);
    txt_putint(PANEL_TX + 1, 18, result.ore_gained, PAL_TXT_GOLD);
    if (result.speed_bonus) {                    // "+12 (+5)" : the bonus for speed
        int w = 1;
        for (int v = result.ore_gained; v >= 10; v /= 10) w++;
        txt_puts(PANEL_TX + 2 + w, 18, "(+", PAL_TXT_WHITE);
        txt_putint(PANEL_TX + 4 + w, 18, result.speed_bonus, PAL_TXT_WHITE);
        int w2 = 1;
        for (int v = result.speed_bonus; v >= 10; v /= 10) w2++;
        txt_puts(PANEL_TX + 4 + w + w2, 18, ")", PAL_TXT_WHITE);
    }
}

static void on_collapse(void)
{
    state = ST_COLLAPSE;
    timer = 0;
    result.frames = elapsed;
    cursor_set_cell(0, 0, false);
    sfx_play(SFX_COLLAPSE);
    txt_clear_rect(0, 1, TILES_W, 1);
    txt_puts_center(1, S(STR_COLLAPSE), PAL_TXT_RED);
    txt_clear_rect(PANEL_TX, 17, TILES_W - PANEL_TX, 3);
}

static void play_update(void)
{
    elapsed++;
    if (ctx.time_budget && (elapsed & 7) == 0 && elapsed <= ctx.time_budget + 8) draw_timebar();
    int dr = 0, dc = 0;
    if (input_nav(KEY_UP))    dr = -1;
    if (input_nav(KEY_DOWN))  dr = 1;
    if (input_nav(KEY_LEFT))  dc = -1;
    if (input_nav(KEY_RIGHT)) dc = 1;
    if (dr || dc) {
        room_cur_r = (room_cur_r + dr + n) % n;
        room_cur_c = (room_cur_c + dc + n) % n;
        cursor_set_cell(room_cur_r, room_cur_c, true);
        sfx_play(SFX_MOVE);
        draw_ghost();
    }

    if (input_hit(KEY_A | KEY_B)) {
        bool primary = input_hit(KEY_A) != 0;
        ActionResult ar = ops->action(room_cur_r, room_cur_c, primary ? ACT_A : ACT_B);
        bool charge = ar.mistake && ops->immediate_mistakes;
        if (charge) start_burst(cell_at(n, room_cur_r, room_cur_c));
        else if (ar.changed) sfx_play(ops->bonus_ore && primary ? SFX_COLLECT : primary ? SFX_PLACE : SFX_MARK);
        if (ar.changed) {
            dirty = true;
            draw_grid();
            draw_tray();
        }
        if (charge && !spend_stability()) return;
        if (ar.changed && ar.solved) { on_solved(); return; }
    }

    if (input_hit(KEY_R) && ops->aux) {
        ops->aux();
        draw_tray();
        draw_ghost();
    }

    watch_conflicts();
    if (state != ST_PLAY) return;
    update_burst();

    if (input_hit(KEY_L)) {
        if (hints_left <= 0) {
            sfx_play(SFX_ERROR);
            show_message(S(STR_NO_HINTS), PAL_TXT_RED);
        } else {
            int r, c;
            int h = ops->hint(&r, &c);
            if (h == HINT_WRONG_PLACEMENT) {
                room_cur_r = r;
                room_cur_c = c;
                cursor_set_cell(room_cur_r, room_cur_c, true);
                sfx_play(SFX_ERROR);
                show_message(S(STR_WRONG_DIG), PAL_TXT_RED);
            } else if (h == HINT_APPLIED) {
                sfx_play(SFX_HINT);
                dirty = true;
                hints_left--;
                result.hints_used++;
                room_cur_r = r;
                room_cur_c = c;
                cursor_set_cell(room_cur_r, room_cur_c, true);
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
        elapsed++;
        if (input_hit(KEY_A)) { state = ST_DONE; outcome = ROOM_ABANDONED; result.frames = elapsed; return outcome; }
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

bool room_take_dirty(void)
{
    // board changes, plus a periodic save so the clock survives a power-off
    bool d = (dirty || (elapsed % 600) == 0) && state == ST_PLAY;
    dirty = false;
    return d;
}

void room_snapshot(RoomSave *out)
{
    memset(out, 0, sizeof *out);
    out->len = (u8)ops->save(out->data);
    out->stability = (u8)stability;
    out->hints_left = (u8)hints_left;
    out->mistakes = (u8)result.mistakes;
    out->hints_used = (u8)result.hints_used;
    out->cur_r = (u8)room_cur_r;
    out->cur_c = (u8)room_cur_c;
    out->elapsed = (u16)(elapsed > 65535 ? 65535 : elapsed);
}
