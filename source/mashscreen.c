#include "mashscreen.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "sound.h"
#include "biome.h"

#define CLR(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

// Layout: "DIG!" and the A button on a band at the top, the wall (7 x 5 rock
// cells) in the middle, the wall's integrity and the time on the pixel canvas
// at the bottom. The clock starts on the first press.
#define WALL_COLS   7
#define WALL_ROWS   5
#define WALL_TX     8
#define WALL_TY     5
#define WALL_CELLS  (WALL_COLS * WALL_ROWS)
#define HEAD_ROWS   3
#define FOOT_TY     17
#define BAR_X       60
#define BAR_W       120
#define WALL_BAR_Y  139
#define TIME_BAR_Y  147
#define MASH_FRAMES 360                       // six seconds once the first blow lands
#define SHATTER_FRAMES 30
#define END_FRAMES  70
#define CHIPS       8
#define BUTTON_SLOT 9                         // the last modal button slot: the chips use 0..7

int mash_hits_left, mash_time_left;

enum { MS_PLAY, MS_SHATTER, MS_WON, MS_LOST };
static int state, timer, hits_needed, ore, shake, press_timer, idle_timer, bank_level;
static bool started;
static uint32_t rng;
static uint8_t order[WALL_CELLS];             // cells in the order they crack, then burst
static uint8_t cracked[WALL_CELLS];

typedef struct { int x, y, vx, vy, life, kind; } Chip;   // 8.8 fixed point
static Chip chips[CHIPS];

static uint32_t next_rand(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

int mash_hits_needed(int difficulty) { return 16 + 2 * clampi(difficulty, 1, 10); }

static void wall_palette(int level)
{
    // the rock darkens as it weakens: sound, cracked, crumbling
    static const u16 fills[3] = { CLR(15, 12, 9), CLR(12, 10, 8), CLR(9, 8, 7) };
    static const u16 cracks[3] = { CLR(11, 9, 7), CLR(9, 7, 6), CLR(7, 6, 5) };
    region_palette(PAL_REGION0, fills[clampi(level, 0, 2)]);
    region_palette(PAL_REGION0 + 1, cracks[clampi(level, 0, 2)]);
}

static void draw_wall(void)
{
    for (int r = 0; r < WALL_ROWS; r++)
        for (int c = 0; c < WALL_COLS; c++) {
            int edges = (r == 0 ? 1 : 0) | (c == WALL_COLS - 1 ? 2 : 0) | (r == WALL_ROWS - 1 ? 4 : 0) | (c == 0 ? 8 : 0);
            grid_cell(r, c, edges, PAL_REGION0 + (cracked[r * WALL_COLS + c] ? 1 : 0));
        }
}

static void draw_bars(void)
{
    int wall_w = hits_needed ? BAR_W * mash_hits_left / hits_needed : 0;
    int time_w = BAR_W * mash_time_left / MASH_FRAMES;
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < BAR_W; x++) {
            canvas_plot(BAR_X + x, WALL_BAR_Y + y, x < wall_w ? CANVAS_ALERT : CANVAS_LINE_DIM);
            canvas_plot(BAR_X + x, TIME_BAR_Y + y, x < time_w ? CANVAS_LINE_LIT : CANVAS_LINE_DIM);
        }
}

static int button_x;

static void draw_header(void)
{
    // "DIG!" and the A button side by side
    const char *s = S(STR_DIG_BANG);
    int w = txt_len(s) * 8 + 4 + 16;
    int x = (SCREEN_WIDTH - w) / 2;
    txt_puts(x / 8, 1, s, PAL_TXT_GOLD);
    button_x = x + txt_len(s) * 8 + 4;
}

// the button dips while pressed
static void button_pressed(bool pressed) { button_sprite(BUTTON_SLOT, BTN_A, button_x, 4 + (pressed ? 2 : 0), true); }

static void spawn_chips(int n, int cx, int cy, int spread)
{
    for (int i = 0; i < CHIPS && n > 0; i++) {
        if (chips[i].life) continue;
        Chip *ch = &chips[i];
        ch->x = cx << 8;
        ch->y = cy << 8;
        ch->vx = (int)(next_rand() % (2 * spread + 1)) - spread;
        ch->vy = -(int)(next_rand() % 512) - 512;
        ch->life = 28 + (int)(next_rand() % 12);
        ch->kind = (int)(next_rand() & 1);
        n--;
    }
}

static void chips_update(void)
{
    for (int i = 0; i < CHIPS; i++) {
        Chip *ch = &chips[i];
        if (!ch->life) { chip_set(i, 0, 0, 0, false); continue; }
        ch->vy += 40;                                     // gravity
        ch->x += ch->vx;
        ch->y += ch->vy;
        ch->life--;
        int y = ch->y >> 8;
        if (ch->life == 0 || y > SCREEN_HEIGHT - 8) { ch->life = 0; chip_set(i, 0, 0, 0, false); continue; }
        chip_set(i, ch->x >> 8, y, ch->kind, true);
    }
}

static void cell_centre(int idx, int *px, int *py)
{
    *px = WALL_TX * 8 + (idx % WALL_COLS) * 16 + 4;
    *py = WALL_TY * 8 + (idx / WALL_COLS) * 16 + 4;
}

static void blow(void)
{
    int done = hits_needed - mash_hits_left;
    int target = order[(done - 1) * WALL_CELLS / hits_needed % WALL_CELLS];
    int px, py;
    cell_centre(target, &px, &py);
    if (!cracked[target]) {
        cracked[target] = 1;
        grid_cell_pal(target / WALL_COLS, target % WALL_COLS, PAL_REGION0 + 1);
        grid_mark(target / WALL_COLS, target % WALL_COLS, MARK_CROSS);
    }
    int level = done * 3 / hits_needed;
    if (level != bank_level) { bank_level = level; wall_palette(level); }
    spawn_chips(3, px, py, 400);
    render_flash_frames(1);
    shake = 4;
    press_timer = 4;
    button_pressed(true);
    sfx_play(SFX_PLACE);
}

static void end_wall(int result)
{
    state = result;
    timer = 0;
    button_sprite(BUTTON_SLOT, BTN_A, 0, 0, false);
    txt_clear_rect(0, 1, TILES_W, 1);
    txt_puts_center(1, S(result == MS_WON ? STR_WALL_BROKEN : STR_WALL_HOLDS), result == MS_WON ? PAL_TXT_GOLD : PAL_TXT_RED);
    if (result == MS_WON) {
        char buf[8];
        int len = 0;
        buf[len++] = '+';
        if (ore >= 100) buf[len++] = (char)('0' + ore / 100);
        if (ore >= 10) buf[len++] = (char)('0' + (ore / 10) % 10);
        buf[len++] = (char)('0' + ore % 10);
        buf[len++] = 'M';
        buf[len] = 0;
        txt_puts_center(FOOT_TY, buf, PAL_TXT_GOLD);
        sfx_play(SFX_SOLVED);
    } else {
        sfx_play(SFX_COLLAPSE);
    }
}

void mash_enter(const RunState *rs, const RunNode *node)
{
    int biome = biome_for_layer(rs->layer, rs->layers);
    rng = rs->seed ^ (0x9E3779B9u * (rs->layer + 1)) ^ 0x6A09E667u;
    if (!rng) rng = 1;
    hits_needed = mash_hits_needed(node->difficulty);
    mash_hits_left = hits_needed;
    mash_time_left = MASH_FRAMES;
    ore = run_reward_ore(node);
    state = MS_PLAY;
    timer = shake = press_timer = idle_timer = bank_level = 0;
    started = false;
    for (int i = 0; i < WALL_CELLS; i++) { order[i] = (uint8_t)i; cracked[i] = 0; }
    for (int i = WALL_CELLS - 1; i > 0; i--) {         // the cracks spread at random
        int j = (int)(next_rand() % (uint32_t)(i + 1));
        uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    for (int i = 0; i < CHIPS; i++) chips[i].life = 0;

    render_clear();
    render_set_biome(biome);
    music_play(MUS_PUZZLE_A);
    modal_fill(0, 0, TILES_W, HEAD_ROWS);
    modal_fill(0, FOOT_TY - 1, TILES_W, TILES_H - FOOT_TY + 1);
    wall_palette(0);
    grid_set_origin(WALL_TX, WALL_TY);
    draw_wall();
    canvas_clear();
    draw_bars();
    canvas_show(true);
    render_canvas_on_top(true);
    chips_clear();
    draw_header();
    button_pressed(false);
}

static void shatter_update(void)
{
    // the cells burst from the centre outwards, marks flashing, chips flying
    if (timer == 0) { render_flash_frames(3); shake = 12; sfx_play(SFX_COLLAPSE); }
    int cx = WALL_COLS / 2, cy = WALL_ROWS / 2;
    for (int i = 0; i < WALL_CELLS; i++) {
        int r = i / WALL_COLS, c = i % WALL_COLS;
        int dist = (r > cy ? r - cy : cy - r) + (c > cx ? c - cx : cx - c);
        int t = timer - dist * 3;
        if (t == 0) {
            grid_cell_blank(r, c);
            grid_mark(r, c, MARK_BURST1);
            int px, py;
            cell_centre(i, &px, &py);
            spawn_chips(1, px, py, 700);
        } else if (t == 4) grid_mark(r, c, MARK_BURST2);
        else if (t == 8) grid_mark(r, c, MARK_BURST3);
        else if (t == 12) grid_mark(r, c, MARK_NONE);
    }
    if (++timer >= SHATTER_FRAMES) end_wall(MS_WON);
}

int mash_update(void)
{
    if (shake) { shake--; render_cells_shift(shake ? (int)(next_rand() % 5) - 2 : 0, shake ? (int)(next_rand() % 3) - 1 : 0); }
    chips_update();
    if (press_timer && --press_timer == 0) button_pressed(false);

    if (state == MS_SHATTER) { shatter_update(); return MASH_RUNNING; }
    if (state != MS_PLAY) {
        if (++timer == END_FRAMES) { button_sprite(BUTTON_SLOT, BTN_A, 13 * 8, 19 * 8 - 4, true); txt_puts(16, 19, "OK", PAL_TXT_WHITE); }
        if (timer >= END_FRAMES && input_hit(KEY_A | KEY_START)) return state == MS_WON ? MASH_WON : MASH_LOST;
        return MASH_RUNNING;
    }

    if (!started) {
        // before the first blow the button presses itself now and then: "go on"
        if (++idle_timer == 30) button_pressed(true);
        else if (idle_timer == 36) { button_pressed(false); idle_timer = 0; }
    } else if (--mash_time_left <= 0) {
        mash_time_left = 0;
        draw_bars();
        end_wall(MS_LOST);
        return MASH_RUNNING;
    }
    if ((mash_time_left & 3) == 0) draw_bars();

    if (input_hit(KEY_A)) {
        started = true;
        mash_hits_left--;
        blow();
        draw_bars();
        if (mash_hits_left <= 0) { state = MS_SHATTER; timer = 0; }
    }
    return MASH_RUNNING;
}
