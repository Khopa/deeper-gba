#include "mapscreen.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "sound.h"
#include "biome.h"

// Geometry: layer k of the window (0 = current) sits at y = 8 + 32k; slot s
// is centred on x = 48 + 72s. Node icons are 16x16 metatiles at tile
// coordinates (5 + 9s, 1 + 4k); the 16 px gap below each layer holds the
// path lines, drawn on the BG2 canvas.
#define WINDOW_LAYERS 4
#define NODE_Y(k)     (24 + 32 * (k))
#define NODE_X(s)     (64 + 64 * (s))
#define NODE_TX(s)    (8 + 8 * (s))
#define NODE_TY(k)    (3 + 4 * (k))
#define CENTER_X(s)   (NODE_X(s) + 7)
#define WALK_FRAMES   40
// the dwarf's 64 px box: his feet on the node's row, standing just left of it
#define DWARF_MAP_X(s) (NODE_X(s) - DWARF_ART - DWARF_PAD - 4)
#define DWARF_MAP_Y(k) (NODE_Y(k) + 8 - DWARF_BOX / 2)   // centred on the node's row

enum { ST_CHOOSE, ST_WALK, ST_ARRIVED };

static int state, timer;
int map_choice;                  // highlighted next slot (not static: read by the emulator scenarios)
static int walk_from_x, walk_from_y, walk_to_x, walk_to_y;

int map_node_icon(const RunNode *n)
{
    switch (n->kind) {
    case NODE_CAMP:  return ICON_FIRECAMP;
    case NODE_CORE:  return ICON_TREASURE;
    case NODE_HINT:  return ICON_KEY;
    case NODE_LIFE:  return ICON_POTION;
    case NODE_RISKY: return ICON_GOLDORE;
    case NODE_CRATES: return ICON_CRATE;
    case NODE_FIGHT: return ICON_SWORD;
    case NODE_WALL: return ICON_WALL;
    default: break;
    }
    switch (n->family) {
    case FAM_VEIN:   return ICON_ORE;
    case FAM_BLOCK:  return ICON_STONEPILE;
    case FAM_TUNNEL: return ICON_LADDER;
    case FAM_LEDGER: return ICON_SCROLL;
    case FAM_NUGGET: return ICON_COAL;
    default:         return ICON_MINING;
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
    case FAM_HEART:  return STR_ROOM_HEART;
    default:         return STR_ROOM_DIG;
    }
}

static void draw_status(const RunState *rs)
{
    txt_clear_rect(0, 0, TILES_W, 1);
    txt_puts(1, 0, S(STR_DEPTH), PAL_TXT_GRAY);
    int x = 1 + txt_len(S(STR_DEPTH)) + 1;
    txt_putint(x, 0, rs->layer + 1, PAL_TXT_WHITE);
    txt_puts(x + 2, 0, "/", PAL_TXT_GRAY);
    txt_putint(x + 3, 0, rs->layers, PAL_TXT_GRAY);
    for (int i = 0; i < RUN_MAX_LIVES; i++)
        txt_puts(12 + i, 0, i < rs->lives ? "\x04" : "\x05", PAL_TXT_RED);
    icon_sprite(0, ICON_ORE, 18 * 8, -4, true);
    txt_putint(20, 0, rs->ore, PAL_TXT_GOLD);
    icon_sprite(1, ICON_KEY, 25 * 8, -4, true);
    txt_putint(27, 0, rs->hints, PAL_TXT_WHITE);
}

// Preview line for the highlighted room: type, difficulty as stars, reward.
static void draw_preview(const RunState *rs)
{
    txt_clear_rect(0, 19, TILES_W, 1);
    icon_sprite(2, 0, 0, 0, false);
    icon_sprite(3, 0, 0, 0, false);
    if (state != ST_CHOOSE) return;
    const RunNode *n = &rs->node[rs->layer + 1][map_choice];
    if (n->kind == NODE_CAMP) {
        txt_puts(1, 19, S(STR_CAMP), PAL_TXT_GOLD);
        txt_puts(TILES_W - 1 - 3, 19, "+", PAL_TXT_RED);
        return;
    }
    if (n->kind == NODE_CRATES) {
        txt_puts(1, 19, S(STR_CRATES), PAL_TXT_GOLD);
        txt_puts(18, 19, "+?", PAL_TXT_GOLD);
        icon_sprite(2, ICON_ORE, 20 * 8 + 1, 19 * 8 - 4, true);
        return;
    }
    if (n->kind == NODE_WALL || n->kind == NODE_FIGHT) {
        txt_puts(1, 19, S(n->kind == NODE_WALL ? STR_WALL : STR_FIGHT), n->kind == NODE_WALL ? PAL_TXT_GOLD : PAL_TXT_RED);
        icon_label(2, ICON_ORE, 18, 19, "+", run_reward_ore(n), PAL_TXT_GOLD);
        return;
    }
    txt_puts(1, 19, S(family_str(n->family)), n->kind == NODE_RISKY ? PAL_TXT_RED : PAL_TXT_WHITE);
    int stars = (n->difficulty + 1) / 2;         // 1..5
    for (int i = 0; i < 5; i++) txt_puts(11 + i, 19, i < stars ? "#" : ".", i < stars ? PAL_TXT_GOLD : PAL_TXT_GRAY);
    int after = icon_label(2, ICON_ORE, 18, 19, "+", run_reward_ore(n), PAL_TXT_GOLD);
    if (n->kind == NODE_HINT) { txt_puts(after, 19, "+", PAL_TXT_WHITE); icon_sprite(3, ICON_KEY, (after + 1) * 8 + 1, 19 * 8 - 4, true); }
    if (n->kind == NODE_LIFE) txt_puts(after, 19, "+", PAL_TXT_RED);
    if (n->kind == NODE_RISKY) txt_puts(after, 19, S(STR_RISK), PAL_TXT_RED);
}

static void draw_window(const RunState *rs)
{
    txt_clear_rect(0, 1, TILES_W, 18);
    canvas_clear();
    int choices = run_next_choices(rs);
    for (int k = 0; k < WINDOW_LAYERS; k++) {
        int l = rs->layer + k;
        if (l >= rs->layers) break;
        for (int s = 0; s < RUN_SLOTS; s++) {
            const RunNode *n = &rs->node[l][s];
            if (!n->present) continue;
            // lit icons keep their own colours: one bank per slot of the next
            // layer (1..3), the current node on bank 4; the rest on the grey ramp
            int style = ICON_DIM, bank = 4;
            if (k == 0 && s == rs->slot) style = ICON_LIT;
            else if (k == 1 && (choices & (1 << s))) { style = ICON_LIT; bank = 1 + s; }
            else if (k == 0) style = ICON_DONE;
            icon_at(NODE_TX(s), NODE_TY(k), map_node_icon(n), style, bank);
        }
        if (k + 1 >= WINDOW_LAYERS || l + 1 >= rs->layers) continue;
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
    dwarf_set(DWARF_MAP_X(rs->slot), DWARF_MAP_Y(0), true);
    dwarf_play(DWARF_IDLE);
}

static void highlight(const RunState *rs)
{
    cursor_set_px(NODE_X(map_choice), NODE_Y(1), true);
    draw_preview(rs);
}

void map_enter(RunState *rs)
{
    render_clear();
    render_palettes_icons();
    render_set_biome(biome_for_layer(rs->layer, rs->layers));
    music_play(MUS_DESCENT);
    state = ST_CHOOSE;
    timer = 0;
    int choices = run_next_choices(rs);
    map_choice = rs->slot;
    if (!(choices & (1 << map_choice)))
        for (map_choice = 0; map_choice < RUN_SLOTS && !(choices & (1 << map_choice)); map_choice++) {}
    draw_status(rs);
    draw_window(rs);
    if (choices) highlight(rs);
    else cursor_set_px(0, 0, false);
}

static void start_walk(const RunState *rs)
{
    state = ST_WALK;
    timer = 0;
    walk_from_x = DWARF_MAP_X(rs->slot);
    walk_from_y = DWARF_MAP_Y(0);
    walk_to_x = DWARF_MAP_X(map_choice);
    walk_to_y = DWARF_MAP_Y(1);
    cursor_set_px(0, 0, false);
    txt_clear_rect(0, 19, TILES_W, 1);
    dwarf_play(DWARF_WALK);
    sfx_play(SFX_STEP);
}

static int drift;

int map_update(RunState *rs)
{
    if ((++drift & 3) == 0) render_backdrop_scroll(0, drift >> 2);   // the rock creeps past slowly
    switch (state) {
    case ST_CHOOSE: {
        int choices = run_next_choices(rs);
        if (!choices) return MAP_RUNNING;
        if (input_hit(KEY_LEFT | KEY_RIGHT)) {
            int dir = input_hit(KEY_LEFT) ? -1 : 1;
            for (int s = map_choice + dir; s >= 0 && s < RUN_SLOTS; s += dir)
                if (choices & (1 << s)) { map_choice = s; break; }
            sfx_play(SFX_MOVE);
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
        if (timer % 10 == 5) sfx_play(SFX_STEP);
        if (timer >= WALK_FRAMES) {
            run_go(rs, map_choice);
            dwarf_play(DWARF_BACK);           // arrived: into the mine
            state = ST_ARRIVED;
            return MAP_ARRIVED;
        }
        return MAP_RUNNING;
    }
    default:
        return MAP_ARRIVED;
    }
}
