#include "room.h"
#include "puzzle.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "sound.h"
#include "biome.h"

// Layout (tiles): grid area is 16x16 tiles from (1,3); smaller grids are
// centred inside it. The right panel starts at tile 18.
// Small-cell rooms (the core's picture, up to 15 x 15 cells of 8 px) use a
// different layout: row clues on the left (7 characters), column clues above,
// a compact panel from tile 23, the header on one row.
#define GRID_AREA_TX 1
#define GRID_AREA_TY 3
#define PANEL_TX     18
#define DWARF_X      196
#define DWARF_Y      28
#define SMALL_GRID_TX    7
#define SMALL_PANEL_TX   23
#define SMALL_DWARF_X    216
#define SMALL_DWARF_Y    136
#define SMALL_TIMEBAR_X  184
#define SMALL_TIMEBAR_Y  57
#define SMALL_TIMEBAR_W  48

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
#define CLR_GOLD_ORE    ((u16)(28 | (26 << 5) | (14 << 10)))   // the ore banks of a revealed picture
#define CLR_WHITE_ORE   ((u16)(31 | (31 << 5) | (26 << 10)))

enum { ST_PLAY, ST_HELP, ST_PAUSE, ST_SOLVED, ST_COLLAPSE, ST_DONE };

static const PuzzleOps *ops;
static bool small;                           // 8 px cells (ops->small_cells)
static int panel_tx, msg_row, dwarf_x, dwarf_y;
static int timebar_x, timebar_y, timebar_w;
static int grid_tx, grid_ty;                 // tile origin of the grid
static RoomContext ctx;
static RoomResult result;
static int state, timer, msg_timer, outcome;
int room_cur_r, room_cur_c;            // the cursor; not static: read by the emulator scenarios
static int n;
static int hints_left, stability;
static int pause_cursor;
static int elapsed;                          // frames spent in play
static bool dirty;
static u8 conflict_age[ROOM_MAX_CELLS];     // frames each cell has been in conflict (255 = already charged)
#define MAX_BURSTS 4
static int burst_cell[MAX_BURSTS], burst_timer[MAX_BURSTS];   // explosion effects
static u8 ghost_cells[2 * 8];
static int ghost_count;
static bool ghost_fits;

static void on_solved(void);
static void on_collapse(void);
static void draw_panel(void);
static void draw_header(void);
static void draw_timebar(void);

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

// Line clues of a small-cell room: rows right-aligned left of the grid, columns
// stacked above it (a two-digit clue takes two rows). Satisfied lines go grey.
static void draw_clues(void)
{
    uint8_t clues[16];
    for (int r = 0; r < n; r++) {
        bool done;
        int count = ops->line_clues(r, clues, &done);
        char buf[16];
        int len = 0;
        for (int k = 0; k < count && len < 14; k++) {
            if (k) buf[len++] = ' ';
            if (clues[k] >= 10) buf[len++] = (char)('0' + clues[k] / 10);
            buf[len++] = (char)('0' + clues[k] % 10);
        }
        buf[len] = 0;
        txt_clear_rect(0, grid_ty + r, grid_tx, 1);
        txt_puts(grid_tx - len, grid_ty + r, buf, done ? PAL_TXT_GRAY : PAL_TXT_WHITE);
    }
    for (int c = 0; c < n; c++) {
        bool done;
        int count = ops->line_clues(n + c, clues, &done);
        int rows = 0;
        for (int k = 0; k < count; k++) rows += clues[k] >= 10 ? 2 : 1;
        txt_clear_rect(grid_tx + c, 1, 1, grid_ty - 1);
        int y = grid_ty - rows;
        int pal = done ? PAL_TXT_GRAY : PAL_TXT_WHITE;
        for (int k = 0; k < count && y >= 1; k++) {
            char d[2] = { 0, 0 };
            if (clues[k] >= 10) { d[0] = (char)('0' + clues[k] / 10); txt_puts(grid_tx + c, y++, d, pal); }
            d[0] = (char)('0' + clues[k] % 10);
            txt_puts(grid_tx + c, y++, d, pal);
        }
    }
}

// Guide lines every five cells, on the pixel layer, in the accent colour
static void draw_group_lines(void)
{
    int x0 = grid_tx * 8, y0 = grid_ty * 8, len = n * 8;
    for (int k = 5; k < n; k += 5) {
        canvas_line(x0 + k * 8 - 1, y0, x0 + k * 8 - 1, y0 + len - 1, CANVAS_LINE_LIT);
        canvas_line(x0, y0 + k * 8 - 1, x0 + len - 1, y0 + k * 8 - 1, CANVAS_LINE_LIT);
    }
}

static void draw_grid(void)
{
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            CellView v;
            ops->cell(r, c, &v);
            if (small) { grid_cell(r, c, v.variant, v.pal); continue; }
            grid_cell(r, c, v.variant * 16 + v.edges, v.pal);
            grid_mark(r, c, v.mark);
        }
    if (small) { draw_clues(); return; }
    ghost_count = 0;
    draw_ghost();
}

// The picture stands out once complete: lit ore on a dark ground
static void draw_reveal(void)
{
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            CellView v;
            ops->cell(r, c, &v);
            if (v.variant == 1) grid_cell(r, c, 4, PAL_CELL_HILITE);   // ore, lit
            else grid_cell(r, c, 5, PAL_TXT_GRAY);                     // dark flat
        }
    draw_clues();
    canvas_clear();
}

// The time bar: what is left of the budget in the accent colour, the rest dim
static void draw_timebar(void)
{
    if (!ctx.time_budget) return;
    int left = ctx.time_budget - elapsed;
    if (left < 0) left = 0;
    int lit = timebar_w * left / ctx.time_budget;
    for (int y = 0; y < TIMEBAR_H; y++)
        for (int x = 0; x < timebar_w; x++)
            canvas_plot(timebar_x + x, timebar_y + y, x < lit ? CANVAS_LINE_LIT : CANVAS_LINE);
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

// Tray: every piece still to place, drawn on the pixel canvas in the lower
// part of the panel (4 columns x 3 rows of 22 px slots, cells of 4 px), the
// one in hand in the accent colour. The canvas rides above the cells in
// these rooms so the panel window does not hide it.
#define TRAY_X      (PANEL_TX * 8)
#define TRAY_Y      88
#define TRAY_SLOT   22
#define TRAY_COLS   4
#define TRAY_ROWS   3
static void draw_tray(void)
{
    if (!ops->tray) return;
    canvas_rect(TRAY_X, TRAY_Y, TRAY_SLOT * TRAY_COLS, TRAY_SLOT * TRAY_ROWS, 0);
    uint8_t rc[2 * 8];
    for (int k = 0; k < TRAY_COLS * TRAY_ROWS; k++) {
        bool current;
        int cells = ops->tray(k, rc, (int)sizeof rc, &current);
        if (cells < 0) break;
        int ox = TRAY_X + (k % TRAY_COLS) * TRAY_SLOT + 1, oy = TRAY_Y + (k / TRAY_COLS) * TRAY_SLOT + 1;
        for (int i = 0; i < cells; i++)
            canvas_rect(ox + rc[2 * i + 1] * 4, oy + rc[2 * i] * 4, 3, 3, current ? CANVAS_LINE_LIT : CANVAS_LINE);
    }
}

// Compact panel (7 characters): hearts, stability, ore, hints, then the keys
static void draw_small_panel(void)
{
    int x = panel_tx;
    txt_clear_rect(x, 1, TILES_W - x, 19);
    modal_fill(x - 1, 1, TILES_W - x + 1, TILES_H - 1);   // a dark window behind the figures, joined to the bar
    for (int i = 0; i < 5; i++)
        txt_puts(x + i, 1, i < ctx.lives ? "\x04" : "\x05", PAL_TXT_RED);
    for (int i = 0; i < ctx.stability && i < 7; i++)
        txt_puts(x + i, 3, "\x03", i < stability ? PAL_TXT_GOLD : PAL_TXT_GRAY);
    txt_puts(x, 5, "M", PAL_TXT_GRAY);
    txt_putint(x + 2, 5, ctx.ore, PAL_TXT_GOLD);
    txt_puts(x, 6, "I", PAL_TXT_GRAY);
    txt_putint(x + 2, 6, hints_left, PAL_TXT_WHITE);
}

static void draw_panel(void)
{
    if (small) { draw_small_panel(); return; }
    int x = PANEL_TX;
    txt_clear_rect(x, 4, TILES_W - x, 16);
    modal_fill(x - 1, 3, TILES_W - x + 1, TILES_H - 3);   // a dark window behind the figures
    txt_puts(x, 4, S(STR_LIVES), PAL_TXT_GRAY);
    for (int i = 0; i < 5; i++)
        txt_puts(x + i, 5, i < ctx.lives ? "\x04" : "\x05", PAL_TXT_RED);
    txt_puts(x, 6, S(STR_STABILITY), PAL_TXT_GRAY);
    for (int i = 0; i < ctx.stability; i++)
        txt_puts(x + i, 7, "\x03", i < stability ? PAL_TXT_GOLD : PAL_TXT_GRAY);
    txt_puts(x, 8, S(STR_ORE), PAL_TXT_GRAY);
    txt_putint(x + txt_len(S(STR_ORE)) + 1, 8, ctx.ore, PAL_TXT_GOLD);
    txt_puts(x, 9, S(STR_HINTS), PAL_TXT_GRAY);
    txt_putint(x + txt_len(S(STR_HINTS)) + 1, 9, hints_left, PAL_TXT_WHITE);
    draw_tray();
}

// The top bar: a dark band (cell layer) with the room's icon and name, the
// biome in its accent colour, and the depth on the right. Messages borrow
// the band's second row. Small-cell rooms get a one-row band without icon.
static void draw_header(void)
{
    const BiomeInfo *bi = biome_info(biome_for_layer(ctx.depth - 1, ctx.max_depth));
    int rows = small ? 1 : 2;
    txt_clear_rect(0, 0, TILES_W, rows);
    modal_fill(0, 0, TILES_W, rows);
    char depth[12];
    int len = 0;
    for (const char *p = S(STR_DEPTH); *p && len < 6; p++) depth[len++] = *p;
    depth[len++] = ' ';
    if (ctx.depth >= 10) depth[len++] = (char)('0' + ctx.depth / 10);
    depth[len++] = (char)('0' + ctx.depth % 10);
    depth[len++] = '/';
    if (ctx.max_depth >= 10) depth[len++] = (char)('0' + ctx.max_depth / 10);
    depth[len++] = (char)('0' + ctx.max_depth % 10);
    depth[len] = 0;
    txt_puts(TILES_W - 1 - len, 0, depth, PAL_TXT_WHITE);
    if (small) {
        txt_puts(1, 0, S(ops->name_str), PAL_TXT_WHITE);
        txt_puts(2 + txt_len(S(ops->name_str)), 0, S(bi->name_str), PAL_TXT_GOLD);
        return;
    }
    node_sprite(ctx.icon, 0, 0, true);
    txt_puts(3, 0, S(ops->name_str), PAL_TXT_WHITE);
    txt_puts(3, 1, S(bi->name_str), PAL_TXT_GOLD);
}

void room_message(const char *s, int pal);

static void show_message(const char *s, int pal)
{
    txt_clear_rect(0, msg_row, TILES_W, 1);
    txt_puts_center(msg_row, s, pal);
    msg_timer = MSG_FRAMES;
}

// Messages sit on the top bar: bring it back when they go
static void clear_message(void)
{
    draw_header();
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
    if (ops->time_scale) ctx.time_budget = ctx.time_budget * ops->time_scale / 100;
    small = ops->small_cells;
    if (small) {
        int clue_rows = TILES_H - 1 - n;              // what fits between the header and the grid
        if (clue_rows > 5) clue_rows = 5;
        grid_tx = SMALL_GRID_TX;
        grid_ty = 1 + clue_rows;
        panel_tx = SMALL_PANEL_TX;
        msg_row = 0;
        dwarf_x = SMALL_DWARF_X;
        dwarf_y = SMALL_DWARF_Y;
        timebar_x = SMALL_TIMEBAR_X;
        timebar_y = SMALL_TIMEBAR_Y;
        timebar_w = SMALL_TIMEBAR_W;
    } else {
        int off = (PUZZLE_MAX_N - n);                 // centre smaller grids (tiles)
        grid_tx = GRID_AREA_TX + off;
        grid_ty = GRID_AREA_TY + off;
        panel_tx = PANEL_TX;
        msg_row = 1;
        dwarf_x = DWARF_X;
        dwarf_y = DWARF_Y;
        timebar_x = TIMEBAR_X;
        timebar_y = TIMEBAR_Y;
        timebar_w = TIMEBAR_W;
    }
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
    if (ops->small_cells) render_palettes_small_room();
    if (ops->small_cells || ops->tray) render_canvas_on_top(true);   // guide lines / the tray show over the tiles
    const BiomeInfo *bi = biome_info(biome_for_layer(ctx.depth - 1, ctx.max_depth));
    render_set_biome(biome_for_layer(ctx.depth - 1, ctx.max_depth));
    music_play(bi->music);
    grid_set_cell_px(small ? 8 : 16);
    grid_set_origin(grid_tx, grid_ty);
    canvas_clear();
    draw_header();
    draw_grid();
    draw_panel();
    if (small) draw_group_lines();
    draw_timebar();
    canvas_show(true);
    dwarf_set(dwarf_x, dwarf_y, true);
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

// "+12 (+5)" : the haul and the bonus for speed; returns the characters used
static int haul_line(int tx, int ty)
{
    txt_puts(tx, ty, "+", PAL_TXT_GOLD);
    txt_putint(tx + 1, ty, result.ore_gained, PAL_TXT_GOLD);
    int w = 1;
    for (int v = result.ore_gained; v >= 10; v /= 10) w++;
    if (!result.speed_bonus) return w + 1;
    txt_puts(tx + 2 + w, ty, "(+", PAL_TXT_WHITE);
    txt_putint(tx + 4 + w, ty, result.speed_bonus, PAL_TXT_WHITE);
    int w2 = 1;
    for (int v = result.speed_bonus; v >= 10; v /= 10) w2++;
    txt_puts(tx + 4 + w + w2, ty, ")", PAL_TXT_WHITE);
    return w + w2 + 5;
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
    msg_timer = 0;
    txt_clear_rect(0, msg_row, TILES_W, 1);
    if (small) {                                 // the picture lights up; the haul comes after
        txt_puts_center(0, S(STR_HEART_REVEALED), PAL_TXT_GOLD);
        draw_reveal();
        return;
    }
    txt_puts_center(1, S(STR_SOLVED), PAL_TXT_GOLD);
    txt_clear_rect(PANEL_TX, 17, TILES_W - PANEL_TX, 3);
    txt_puts(PANEL_TX, 17, S(STR_ORE_FOUND), PAL_TXT_GRAY);
    haul_line(PANEL_TX, 18);
}

// The haul after a small room, on the header row: "+150 (+20)" and the key to go on
static void small_haul(void)
{
    txt_clear_rect(0, 0, TILES_W, 1);
    int w = haul_line(1, 0);
    button_sprite(0, BTN_A, (w + 3) * 8, 0, true);
    txt_puts(w + 6, 0, "OK", PAL_TXT_WHITE);
}

static void on_collapse(void)
{
    state = ST_COLLAPSE;
    timer = 0;
    result.frames = elapsed;
    cursor_set_cell(0, 0, false);
    sfx_play(SFX_COLLAPSE);
    msg_timer = 0;
    txt_clear_rect(0, msg_row, TILES_W, 1);
    txt_puts_center(msg_row, S(STR_COLLAPSE), PAL_TXT_RED);
    if (!small) txt_clear_rect(PANEL_TX, 17, TILES_W - PANEL_TX, 3);
}

// --- modals: help (SELECT) and pause (START) -----------------------------------------
// A full-screen box on the cell layer hides the room; sprites and the time
// bar are hidden too, and the clock does not run. Closing redraws everything.

static int modal_icons;                      // button sprites in use by the open modal

static void modal_open(void)
{
    modal_icons = 0;
    button_sprites_clear();
    node_sprite(0, 0, 0, false);
    cursor_set_cell(0, 0, false);
    dwarf_set(0, 0, false);
    canvas_show(false);
    txt_clear();
    modal_fill(0, 0, TILES_W, TILES_H);
}

static void close_modal(void)
{
    sfx_play(SFX_MARK);
    state = ST_PLAY;
    button_sprites_clear();
    modal_clear();
    txt_clear();
    canvas_clear();
    draw_header();
    draw_grid();
    draw_panel();
    if (small) draw_group_lines();
    draw_timebar();
    canvas_show(true);
    dwarf_set(dwarf_x, dwarf_y, true);
    cursor_set_cell(room_cur_r, room_cur_c, true);
}

// Word-wrap `s` into rows of at most `width` characters from (tx, ty); returns rows used
static int puts_wrapped(int tx, int ty, int width, const char *s, int pal)
{
    int rows = 0;
    while (*s) {
        int len = txt_len(s), cut = len;
        if (len > width) {
            cut = width;
            while (cut > 0 && s[cut] != ' ') cut--;
            if (cut == 0) cut = width;
        }
        char line[40];
        int n = cut < 39 ? cut : 39;
        for (int i = 0; i < n; i++) line[i] = s[i];
        line[n] = 0;
        txt_puts(tx, ty + rows, line, pal);
        rows++;
        s += cut;
        while (*s == ' ') s++;
    }
    return rows;
}

// A control line: button icon (2x2) then its label; key labels in the string
// table start with the key letter and a space, which the icon replaces.
static void modal_icon(int tx, int ty, int button)
{
    button_sprite(modal_icons++, button, tx * 8, ty * 8 - 4, true);
}

static void control_line(int tx, int ty, int button, const char *label, bool strip_key)
{
    modal_icon(tx, ty, button);
    if (strip_key && label[0] && label[1] == ' ') label += 2;
    txt_puts(tx + 3, ty, label, PAL_TXT_WHITE);
}

static void open_help(void)
{
    state = ST_HELP;
    modal_open();
    txt_puts_center(1, S(ops->name_str), PAL_TXT_GOLD);
    int used = puts_wrapped(2, 3, TILES_W - 4, S(ops->help_str), PAL_TXT_WHITE);
    int y = 3 + used + 1;
    txt_puts(2, y, S(STR_CONTROLS), PAL_TXT_GOLD);
    y++;
    control_line(2, y, BTN_DPAD, S(STR_KEY_MOVE), false);
    control_line(16, y, BTN_L, S(STR_KEY_L_HINT), true);
    y += 2;
    control_line(2, y, BTN_A, S(ops->key_a_str), true);
    if (ops->aux) control_line(16, y, BTN_R, S(ops->key_r_str), true);
    y += 2;
    control_line(2, y, BTN_B, S(ops->key_b_str), true);
    y += 2;
    control_line(2, y, BTN_SELECT, S(STR_KEY_HELP), false);
    control_line(16, y, BTN_START, S(STR_KEY_PAUSE), false);
    modal_icon(11, 18, BTN_B);
    txt_puts(14, 18, S(STR_CLOSE), PAL_TXT_GRAY);
}

static void draw_pause_menu(void)
{
    static const int labels[3] = { STR_RESUME, STR_GIVE_UP, STR_SAVE_QUIT };
    for (int i = 0; i < 3; i++) {
        int row = 8 + 2 * i;
        txt_clear_rect(0, row, TILES_W, 1);
        txt_puts_center(row, S(labels[i]), i == pause_cursor ? PAL_TXT_GOLD : i == 1 ? PAL_TXT_RED : PAL_TXT_WHITE);
        if (i == pause_cursor) txt_puts((TILES_W - txt_len(S(labels[i]))) / 2 - 2, row, ">", PAL_TXT_GOLD);
    }
}

static void open_pause(void)
{
    state = ST_PAUSE;
    pause_cursor = 0;
    modal_open();
    sfx_play(SFX_MARK);
    txt_puts_center(4, S(STR_PAUSE), PAL_TXT_GOLD);
    draw_pause_menu();
    modal_icon(6, 16, BTN_A);
    txt_puts(9, 16, S(STR_CHOOSE), PAL_TXT_GRAY);
    modal_icon(17, 16, BTN_B);
    txt_puts(20, 16, S(STR_RESUME), PAL_TXT_GRAY);
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

    if (input_hit(KEY_SELECT)) { open_help(); return; }
    if (input_hit(KEY_START))  { open_pause(); return; }

    if (msg_timer && --msg_timer == 0) clear_message();
}

int room_update(void)
{
    switch (state) {
    case ST_PLAY:
        play_update();
        return ROOM_RUNNING;
    case ST_HELP:                                 // the clock is stopped while reading
        if (input_hit(KEY_B | KEY_SELECT | KEY_START | KEY_A)) close_modal();
        return ROOM_RUNNING;
    case ST_PAUSE:
        if (input_hit(KEY_UP | KEY_DOWN)) {
            pause_cursor = (pause_cursor + (input_hit(KEY_UP) ? 2 : 1)) % 3;
            sfx_play(SFX_MOVE);
            draw_pause_menu();
        }
        if (input_hit(KEY_B | KEY_START)) { close_modal(); return ROOM_RUNNING; }
        if (input_hit(KEY_A)) {
            if (pause_cursor == 0) { close_modal(); return ROOM_RUNNING; }
            sfx_play(SFX_MARK);
            state = ST_DONE;
            result.frames = elapsed;
            outcome = pause_cursor == 1 ? ROOM_ABANDONED : ROOM_QUIT;
            return outcome;
        }
        return ROOM_RUNNING;
    case ST_SOLVED:
        if (small && timer < SOLVED_FRAMES && (timer & 7) == 0)        // the ore pulses
            region_palette(PAL_CELL_HILITE, (timer & 8) ? CLR_WHITE_ORE : CLR_GOLD_ORE);
        if (++timer == SOLVED_FRAMES) {
            dwarf_play(DWARF_IDLE);
            if (small) { region_palette(PAL_CELL_HILITE, CLR_GOLD_ORE); small_haul(); }
            else txt_puts(PANEL_TX, 19, S(STR_NEXT), PAL_TXT_WHITE);
        }
        if (timer >= SOLVED_FRAMES && input_hit(KEY_A | KEY_START)) { state = ST_DONE; outcome = ROOM_DONE; }
        return outcome;
    case ST_COLLAPSE:
        if (++timer >= COLLAPSE_FRAMES && input_hit(KEY_A | KEY_START)) { state = ST_DONE; outcome = ROOM_COLLAPSED; }
        if (timer == COLLAPSE_FRAMES) {
            if (small) { button_sprite(0, BTN_A, panel_tx * 8, 18 * 8 - 4, true); txt_puts(panel_tx + 3, 18, "OK", PAL_TXT_WHITE); }
            else txt_puts(PANEL_TX, 19, S(STR_NEXT), PAL_TXT_WHITE);
        }
        return outcome;
    default:
        return outcome;
    }
}

const RoomResult *room_result(void) { return &result; }

void room_message(const char *s, int pal) { show_message(s, pal); }

bool room_paused(void) { return state == ST_HELP || state == ST_PAUSE; }

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
