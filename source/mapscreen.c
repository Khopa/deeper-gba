#include "mapscreen.h"
#include "render.h"
#include "input.h"
#include "lang.h"

// Geometry: layer k of the window (0 = current) sits at y = 8 + 32k; slot s
// is centred on x = 48 + 72s. Node icons are 16x16 metatiles at tile
// coordinates (5 + 9s, 1 + 4k); the 16 px gap below each layer holds the
// path lines, drawn on the BG2 canvas.
#define WINDOW_LAYERS 5
#define NODE_Y(k)     (8 + 32 * (k))
#define NODE_X(s)     (40 + 72 * (s))
#define NODE_TX(s)    (5 + 9 * (s))
#define NODE_TY(k)    (1 + 4 * (k))
#define CENTER_X(s)   (NODE_X(s) + 7)
#define WALK_FRAMES   40

enum { ST_CHOOSE, ST_WALK, ST_ARRIVED };

static int state, timer;
static int choice;               // highlighted next slot
static int walk_from_x, walk_from_y, walk_to_x, walk_to_y;

int map_node_icon(const RunNode *n)
{
    switch (n->kind) {
    case NODE_CAMP:  return ICON_CAMP;
    case NODE_CORE:  return ICON_CORE;
    case NODE_HINT:  return ICON_HINT;
    case NODE_LIFE:  return ICON_LIFE;
    case NODE_RISKY: return ICON_RISKY;
    default: break;
    }
    switch (n->family) {
    case FAM_VEIN:   return ICON_VEIN;
    case FAM_BLOCK:  return ICON_BLOCK;
    case FAM_TUNNEL: return ICON_TUNNEL;
    case FAM_LEDGER: return ICON_LEDGER;
    case FAM_NUGGET: return ICON_NUGGET;
    default:         return ICON_DIG;
    }
}

static int family_str(int family)
{
    switch (family) {
    case FAM_VEIN:   return STR_ROOM_VEIN;
    case FAM_BLOCK:  return STR_ROOM_BLOCK;
    case FAM_TUNNEL: return STR_ROOM_TUNNEL;
    case FAM_LEDGER: return STR_ROOM_LEDGER;
    case FAM_NUGGET: return STR_ROOM_NUGGET;
    default:         return STR_ROOM_DIG;
    }
}

static void draw_status(const RunState *rs)
{
    txt_clear_rect(0, 0, TILES_W, 1);
    txt_puts(1, 0, S(STR_DEPTH), PAL_TXT_GRAY);
    int x = 1 + txt_len(S(STR_DEPTH)) + 1;
    txt_putint(x, 0, rs->layer + 1, PAL_TXT_WHITE);
    txt_puts(x + 2, 0, "/30", PAL_TXT_GRAY);
    for (int i = 0; i < RUN_MAX_LIVES; i++)
        txt_puts(12 + i, 0, i < rs->lives ? "\x04" : "\x05", PAL_TXT_RED);
    txt_puts(19, 0, "M", PAL_TXT_GRAY);
    txt_putint(20, 0, rs->ore, PAL_TXT_GOLD);
    txt_puts(26, 0, "I", PAL_TXT_GRAY);
    txt_putint(27, 0, rs->hints, PAL_TXT_WHITE);
}

// Preview line for the highlighted room: type, difficulty as stars, reward.
static void draw_preview(const RunState *rs)
{
    txt_clear_rect(0, 19, TILES_W, 1);
    if (state != ST_CHOOSE) return;
    const RunNode *n = &rs->node[rs->layer + 1][choice];
    if (n->kind == NODE_CAMP) {
        txt_puts(1, 19, S(STR_CAMP), PAL_TXT_GOLD);
        txt_puts(TILES_W - 1 - 3, 19, "+\x04", PAL_TXT_RED);
        return;
    }
    txt_puts(1, 19, S(family_str(n->family)), n->kind == NODE_RISKY ? PAL_TXT_RED : PAL_TXT_WHITE);
    int stars = (n->difficulty + 1) / 2;         // 1..5
    for (int i = 0; i < 5; i++) txt_puts(11 + i, 19, i < stars ? "#" : ".", i < stars ? PAL_TXT_GOLD : PAL_TXT_GRAY);
    char buf[8];
    int ore = run_reward_ore(n);
    int len = 0;
    buf[len++] = '+';
    if (ore >= 100) buf[len++] = (char)('0' + ore / 100);
    if (ore >= 10) buf[len++] = (char)('0' + (ore / 10) % 10);
    buf[len++] = (char)('0' + ore % 10);
    buf[len++] = 'M';
    buf[len] = 0;
    txt_puts(18, 19, buf, PAL_TXT_GOLD);
    if (n->kind == NODE_HINT) txt_puts(24, 19, "+I", PAL_TXT_WHITE);
    if (n->kind == NODE_LIFE) txt_puts(24, 19, "+\x04", PAL_TXT_RED);
    if (n->kind == NODE_RISKY) txt_puts(24, 19, S(STR_RISK), PAL_TXT_RED);
}

static void draw_window(const RunState *rs)
{
    txt_clear_rect(0, 1, TILES_W, 18);
    canvas_clear();
    int choices = run_next_choices(rs);
    for (int k = 0; k < WINDOW_LAYERS; k++) {
        int l = rs->layer + k;
        if (l >= RUN_LAYERS) break;
        for (int s = 0; s < RUN_SLOTS; s++) {
            const RunNode *n = &rs->node[l][s];
            if (!n->present) continue;
            int pal = PAL_NODE_DIM;
            if (k == 0 && s == rs->slot) pal = PAL_NODE_LIT;
            else if (k == 1 && (choices & (1 << s))) pal = PAL_NODE_LIT;
            else if (k == 0) pal = PAL_NODE_DONE;
            map_icon(NODE_TX(s), NODE_TY(k), map_node_icon(n), pal);
        }
        if (k + 1 >= WINDOW_LAYERS || l + 1 >= RUN_LAYERS) continue;
        for (int a = 0; a < RUN_SLOTS; a++)
            for (int b = 0; b < RUN_SLOTS; b++) {
                if (!(rs->edges[l] & (1 << (a * RUN_SLOTS + b)))) continue;
                int color = CANVAS_LINE;
                if (k == 0 && a == rs->slot) color = CANVAS_LINE_LIT;
                else if (k == 0) color = CANVAS_LINE_DIM;
                int x0 = CENTER_X(a), y0 = NODE_Y(k) + 16, x1 = CENTER_X(b), y1 = NODE_Y(k + 1) - 1;
                canvas_line(x0, y0, x1, y1, color);
                canvas_line(x0 + 1, y0, x1 + 1, y1, color);
            }
    }
    canvas_show(true);
    dwarf_set(NODE_X(rs->slot) - 18, NODE_Y(0), true);
    dwarf_play(DWARF_IDLE);
}

static void highlight(const RunState *rs)
{
    cursor_set_px(NODE_X(choice), NODE_Y(1), true);
    draw_preview(rs);
}

void map_enter(RunState *rs)
{
    render_clear();
    render_palettes_map();
    state = ST_CHOOSE;
    timer = 0;
    int choices = run_next_choices(rs);
    choice = rs->slot;
    if (!(choices & (1 << choice)))
        for (choice = 0; choice < RUN_SLOTS && !(choices & (1 << choice)); choice++) {}
    draw_status(rs);
    draw_window(rs);
    if (choices) highlight(rs);
    else cursor_set_px(0, 0, false);
}

static void start_walk(const RunState *rs)
{
    state = ST_WALK;
    timer = 0;
    walk_from_x = NODE_X(rs->slot) - 18;
    walk_from_y = NODE_Y(0);
    walk_to_x = NODE_X(choice) - 18;
    walk_to_y = NODE_Y(1);
    cursor_set_px(0, 0, false);
    txt_clear_rect(0, 19, TILES_W, 1);
    dwarf_play(DWARF_DIG);
}

int map_update(RunState *rs)
{
    switch (state) {
    case ST_CHOOSE: {
        int choices = run_next_choices(rs);
        if (!choices) return MAP_RUNNING;
        if (input_hit(KEY_LEFT | KEY_RIGHT)) {
            int dir = input_hit(KEY_LEFT) ? -1 : 1;
            for (int s = choice + dir; s >= 0 && s < RUN_SLOTS; s += dir)
                if (choices & (1 << s)) { choice = s; break; }
            highlight(rs);
        }
        if (input_hit(KEY_A | KEY_START)) start_walk(rs);
        return MAP_RUNNING;
    }
    case ST_WALK: {
        timer++;
        int x = walk_from_x + (walk_to_x - walk_from_x) * timer / WALK_FRAMES;
        int y = walk_from_y + (walk_to_y - walk_from_y) * timer / WALK_FRAMES;
        dwarf_set(x, y, true);
        if (timer >= WALK_FRAMES) {
            run_go(rs, choice);
            state = ST_ARRIVED;
            return MAP_ARRIVED;
        }
        return MAP_RUNNING;
    }
    default:
        return MAP_ARRIVED;
    }
}
