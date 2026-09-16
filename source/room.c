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
#define DWARF_X      (SCREEN_WIDTH - DWARF_BOX + 4)     // the 64 px box: bottom of the panel, right edge
#define DWARF_Y      (TIMEBAR_Y - 6 - DWARF_ART - DWARF_PAD)
#define SMALL_GRID_TX    7
#define SMALL_PANEL_TX   23
#define SMALL_DWARF_X    (SCREEN_WIDTH - DWARF_BOX + 4)
#define SMALL_DWARF_Y    (SCREEN_HEIGHT - DWARF_ART - DWARF_PAD)
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
#define INTRO_STEP      2       // frames per diagonal of cells revealed
#define SWEEP_STEP      2       // frames per diagonal in the gold sweep / vanish
#define FLASH_PERIOD    40      // frames between screen flashes under time pressure
#define FLASH_FRAMES    3
#define COUNT_FRAMES    45      // the summary counts its ore over this many frames
#define CLR_GOLD_ORE    ((u16)(28 | (26 << 5) | (14 << 10)))   // the ore banks of a revealed picture
#define CLR_WHITE_ORE   ((u16)(31 | (31 << 5) | (26 << 10)))

// (PLAY stays 0: the scenarios read room_state to know when a room accepts keys)
enum { ST_PLAY, ST_HELP, ST_PAUSE, ST_SOLVED, ST_COLLAPSE, ST_DONE, ST_INTRO, ST_SUMMARY };

static const PuzzleOps *ops;
static bool small;                           // 8 px cells (ops->small_cells)
static int panel_tx, msg_row, dwarf_x, dwarf_y;
static int timebar_x, timebar_y, timebar_w;
static int grid_tx, grid_ty;                 // tile origin of the grid
static RoomContext ctx;
static RoomResult result;
int room_state;                       // not static: the emulator scenarios wait for ST_PLAY
static int timer, msg_timer, outcome;
static int reveal;                    // intro: diagonals of cells shown so far (-1 = none)
static int counter;                   // summary: the ore counted up so far
static bool flashing;
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
static void modal_open(void);
static void modal_icon(int tx, int ty, int button);

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
            if (room_state == ST_INTRO && r + c > reveal) { grid_cell_blank(r, c); continue; }
            CellView v;
            ops->cell(r, c, &v);
            if (small) { grid_cell(r, c, v.variant, v.pal); continue; }
            grid_cell(r, c, v.variant * 16 + v.edges, v.pal);
            grid_mark(r, c, v.mark);
        }
    if (small) { draw_clues(); return; }
    if (room_state == ST_INTRO) return;
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
    int color = left * 10 < ctx.time_budget ? CANVAS_ALERT : CANVAS_LINE_LIT;
    for (int y = 0; y < TIMEBAR_H; y++)
        for (int x = 0; x < timebar_w; x++)
            canvas_plot(timebar_x + x, timebar_y + y, x < lit ? color : CANVAS_LINE);
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
    icon_sprite(0, ICON_ORE, x * 8, 5 * 8 - 4, true);
    txt_putint(x + 2, 5, ctx.ore, PAL_TXT_GOLD);
    icon_sprite(1, ICON_KEY, x * 8, 7 * 8 - 4, true);
    txt_putint(x + 2, 7, hints_left, PAL_TXT_WHITE);
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
    icon_sprite(0, ICON_ORE, x * 8, 8 * 8 - 4, true);
    txt_putint(x + 2, 8, ctx.ore, PAL_TXT_GOLD);
    icon_sprite(1, ICON_KEY, (x + 7) * 8, 8 * 8 - 4, true);
    txt_putint(x + 9, 8, hints_left, PAL_TXT_WHITE);
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
    icon_sprite(2, ctx.icon, 0, 0, true);
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
    room_state = ST_PLAY;
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
    if (e->hdr->family == FAM_VEIN) {              // two gems of the depth, never the same pair twice running
        static const int gems[5] = { ICON_AMETHYST, ICON_CRYSTAL, ICON_DIAMOND, ICON_RUBY, ICON_SAPPHIRE };
        u32 h = (u32)ctx.depth * 2654435761u ^ (u32)e->hdr->size * 40503u;
        int a = (int)(h % 5), b = (int)((h >> 8) % 4);
        if (b >= a) b++;
        render_set_gems(gems[a], gems[b]);
    }
    if (ops->small_cells || ops->tray) render_canvas_on_top(true);   // guide lines / the tray show over the tiles
    render_set_biome(biome_for_layer(ctx.depth - 1, ctx.max_depth));
    if (ops->family == FAM_HEART) music_play(MUS_CORE);
    else {                                        // one of the puzzle themes, never the one of the previous room
        static int last_theme = MUS_NONE;
        u32 pick = (ctx.depth * 2654435761u) >> 28;
        int id = MUS_PUZZLE_A + (int)(pick % MUS_PUZZLE_COUNT);
        if (id == last_theme) id = MUS_PUZZLE_A + (id - MUS_PUZZLE_A + 1) % MUS_PUZZLE_COUNT;
        last_theme = id;
        music_play(id);
    }
    grid_set_cell_px(small ? 8 : 16);
    grid_set_origin(grid_tx, grid_ty);
    canvas_clear();
    draw_header();
    draw_grid();
    draw_panel();
    if (small) draw_group_lines();
    draw_timebar();
    canvas_show(true);
    dwarf_set(dwarf_x, dwarf_y, !ops->tray);         // the tray takes his corner
    dwarf_play(DWARF_IDLE);
    // the cells come in along the diagonals before the room takes keys
    room_state = ST_INTRO;
    reveal = -1;
    timer = 0;
    flashing = false;
    draw_grid();
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
static void on_solved(void)
{
    room_state = ST_SOLVED;
    timer = 0;
    result.solved = true;
    result.frames = elapsed;
    result.ore_gained = ore_reward();
    cursor_set_cell(0, 0, false);
    dwarf_play(DWARF_WALK);
    sfx_play(SFX_SOLVED);
    render_flash(false);
    flashing = false;
    msg_timer = 0;
    txt_clear_rect(0, msg_row, TILES_W, 1);
    if (small) {                                 // the picture lights up and pulses
        txt_puts_center(0, S(STR_HEART_REVEALED), PAL_TXT_GOLD);
        draw_reveal();
        return;
    }
    txt_puts_center(1, S(STR_SOLVED), PAL_TXT_GOLD);
}

// --- the summary: how the room went, its ore counted up ---------------------------
// Rating: three gems for a clean, quick room (no mistake, no hint, more than
// half the budget left), two for a clean one, one otherwise.
static int rating(void)
{
    if (ops->bonus_ore) return result.ore_gained > 0 ? 2 : 1;
    if (result.mistakes) return 1;
    if (result.hints_used) return 2;
    return (ctx.time_budget && result.speed_bonus * 2 >= ctx.reward_ore) ? 3 : 2;
}

static void draw_counter(void)
{
    txt_clear_rect(0, 15, TILES_W, 1);
    char buf[12];
    int len = 0, v = counter;
    buf[len++] = '+';
    char digits[8];
    int nd = 0;
    do { digits[nd++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (nd) buf[len++] = digits[--nd];
    buf[len] = 0;
    txt_puts_center(15, buf, PAL_TXT_GOLD);
    icon_sprite(0, ICON_ORE, (TILES_W + len) * 4 + 2, 15 * 8 - 4, true);
}

static void open_summary(void)
{
    room_state = ST_SUMMARY;
    timer = 0;
    counter = 0;
    modal_open();
    dwarf_set(0, 0, false);
    txt_puts_center(1, S(ops->name_str), PAL_TXT_GOLD);
    int gems = rating();
    for (int i = 0; i < gems; i++) mark_at(TILES_W / 2 - gems + 2 * i, 3, MARK_GEM);
    int y = 6;
    if (ctx.time_budget) {                       // the time bar as glyphs: what was left of the budget
        int left = ctx.time_budget - elapsed;
        if (left < 0) left = 0;
        int lit = 16 * left / ctx.time_budget;
        txt_puts(3, y, S(STR_TIME), PAL_TXT_GRAY);
        for (int i = 0; i < 16; i++) txt_puts(11 + i, y, "\x03", i < lit ? PAL_TXT_GOLD : PAL_TXT_GRAY);
        y += 2;
    }
    txt_puts(3, y, S(STR_MISTAKES), PAL_TXT_GRAY);
    txt_putint(13, y, result.mistakes, result.mistakes ? PAL_TXT_RED : PAL_TXT_WHITE);
    icon_sprite(1, ICON_KEY, 17 * 8, y * 8 - 4, true);
    txt_putint(20, y, result.hints_used, PAL_TXT_WHITE);
    y += 2;
    int base = ops->bonus_ore ? result.ore_gained : ctx.reward_ore;
    int penalty = ops->bonus_ore ? 0 : ctx.reward_ore + result.speed_bonus - result.ore_gained;
    txt_puts(3, y, S(STR_ORE_FOUND), PAL_TXT_GRAY);
    txt_putint(18, y, base, PAL_TXT_WHITE);
    y++;
    if (!ops->bonus_ore) {
        txt_puts(3, y, S(STR_SPEED_BONUS), PAL_TXT_GRAY);
        txt_puts(18, y, "+", PAL_TXT_WHITE);
        txt_putint(19, y, result.speed_bonus, PAL_TXT_WHITE);
        y++;
        if (penalty > 0) {
            txt_puts(3, y, S(STR_PENALTY), PAL_TXT_GRAY);
            txt_puts(18, y, "-", PAL_TXT_RED);
            txt_putint(19, y, penalty, PAL_TXT_RED);
        }
    }
    draw_counter();
    modal_icon(11, 18, BTN_A);
    txt_puts(14, 18, S(STR_NEXT) + 2, PAL_TXT_GRAY);
}

static void summary_update(void)
{
    int total = result.ore_gained;
    if (counter < total) {
        int step = total / COUNT_FRAMES + 1;
        counter += step;
        if (counter > total) counter = total;
        draw_counter();
        if ((timer & 3) == 0) sfx_play(SFX_COLLECT);
        if (input_hit(KEY_A | KEY_START)) { counter = total; draw_counter(); }
        timer++;
        return;
    }
    if (input_hit(KEY_A | KEY_START)) { room_state = ST_DONE; outcome = ROOM_DONE; }
}

static void on_collapse(void)
{
    room_state = ST_COLLAPSE;
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
    icon_sprites_clear();
    cursor_set_cell(0, 0, false);
    dwarf_set(0, 0, false);
    canvas_show(false);
    txt_clear();
    modal_fill(0, 0, TILES_W, TILES_H);
}

static void close_modal(void)
{
    sfx_play(SFX_MARK);
    room_state = ST_PLAY;
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
    dwarf_set(dwarf_x, dwarf_y, !ops->tray);
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
    room_state = ST_HELP;
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
    room_state = ST_PAUSE;
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

// The last tenth of the budget: the screen flashes white every few dozen frames
static void time_pressure(void)
{
    int left = ctx.time_budget - elapsed;
    bool pressure = ctx.time_budget && left > 0 && left * 10 < ctx.time_budget;
    if (!pressure) {
        if (flashing) { render_flash(false); flashing = false; }
        return;
    }
    int phase = elapsed % FLASH_PERIOD;
    if (phase == 0) { render_flash(true); flashing = true; sfx_play(SFX_STEP); }
    else if (phase == FLASH_FRAMES && flashing) { render_flash(false); flashing = false; }
}

static void play_update(void)
{
    elapsed++;
    if (ctx.time_budget && (elapsed & 7) == 0 && elapsed <= ctx.time_budget + 8) draw_timebar();
    time_pressure();
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
    if (room_state != ST_PLAY) return;
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
    switch (room_state) {
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
            room_state = ST_DONE;
            result.frames = elapsed;
            outcome = pause_cursor == 1 ? ROOM_ABANDONED : ROOM_QUIT;
            return outcome;
        }
        return ROOM_RUNNING;
    case ST_INTRO: {
        if (++timer % INTRO_STEP) return ROOM_RUNNING;
        reveal++;
        if ((reveal & 1) == 0) sfx_play(SFX_STEP);
        if (reveal >= 2 * n - 2) {
            room_state = ST_PLAY;
            draw_grid();
            cursor_set_cell(room_cur_r, room_cur_c, true);
            return ROOM_RUNNING;
        }
        draw_grid();
        return ROOM_RUNNING;
    }
    case ST_SOLVED: {
        if (small) {                              // the picture pulses, then the summary
            if (timer < SOLVED_FRAMES && (timer & 7) == 0)
                region_palette(PAL_CELL_HILITE, (timer & 8) ? CLR_WHITE_ORE : CLR_GOLD_ORE);
            if (++timer >= SOLVED_FRAMES) { region_palette(PAL_CELL_HILITE, CLR_GOLD_ORE); dwarf_play(DWARF_IDLE); open_summary(); }
            return ROOM_RUNNING;
        }
        // a gold sweep along the diagonals, then the cells vanish the same way
        int diagonals = 2 * n - 1;
        if (timer % SWEEP_STEP == 0) {
            int d = timer / SWEEP_STEP;
            if (d < diagonals) {
                for (int r = 0; r < n; r++) { int c = d - r; if (c >= 0 && c < n) grid_cell_pal(r, c, PAL_CELL_HILITE); }
                if ((d & 1) == 0) sfx_play(SFX_MARK);
            } else if (d < 2 * diagonals) {
                int e = d - diagonals;
                for (int r = 0; r < n; r++) { int c = e - r; if (c >= 0 && c < n) grid_cell_blank(r, c); }
            } else {
                dwarf_play(DWARF_IDLE);
                open_summary();
                return ROOM_RUNNING;
            }
        }
        timer++;
        return ROOM_RUNNING;
    }
    case ST_SUMMARY:
        summary_update();
        return outcome;
    case ST_COLLAPSE:
        if (++timer >= COLLAPSE_FRAMES && input_hit(KEY_A | KEY_START)) { room_state = ST_DONE; outcome = ROOM_COLLAPSED; }
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

bool room_paused(void) { return room_state == ST_HELP || room_state == ST_PAUSE || room_state == ST_INTRO || room_state == ST_SUMMARY; }

bool room_take_dirty(void)
{
    // board changes, plus a periodic save so the clock survives a power-off
    bool d = (dirty || (elapsed % 600) == 0) && room_state == ST_PLAY;
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
