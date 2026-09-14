// Rendering: Mode 0, three regular backgrounds and a handful of sprites.
//   BG0  text + marks   charblock 0 (font, marks), screenblock 12, priority 0
//   BG1  grid cells     charblock 1 (cells, tiles 0..139), screenblock 13, priority 1
//   BG2  canvas         charblocks 2-3 (600 tiles), screenblock 14, priority 2
//   BG3  biome backdrop charblock 3 (tiles 96..511: the biome's block variants, 16/32/64 px), screenblock 15, priority 3
// (screenblocks 12..15 are the top of charblock 1, past the cell tiles)
//   OBJ  cursor (tiles 0-7), dwarf (8-23), merchant (24-55), menu icons (56-119), logo (120-183)
// Colours are palette-bank swaps on shared tiles: each rock region of a
// puzzle owns a bank, text colours are banks too.
#ifndef RENDER_H
#define RENDER_H

#include "common.h"

// BG palette banks
enum {
    PAL_TXT_WHITE = 0,
    PAL_REGION0   = 1,          // 1..8: rock regions
    PAL_CELL_CONFLICT = 9,
    PAL_CELL_HILITE   = 10,
    PAL_BACKDROP  = 11,                // biome tiles: indices 1..15 from the PNG
    PAL_MARKS     = 12,
    PAL_TXT_GRAY  = 13,
    PAL_TXT_GOLD  = 14,
    PAL_TXT_RED   = 15,
};

// Overlay marks (metatile order of assets/marks.png)
enum { MARK_NONE = 0, MARK_CROSS, MARK_DIG, MARK_ALERT, MARK_GEM, MARK_ORE_LIGHT, MARK_ORE_DARK,
       MARK_GHOST_OK, MARK_GHOST_BAD, MARK_BURST1, MARK_BURST2, MARK_BURST3,
       MARK_DIGIT1, MARK_DIGIT12 = MARK_DIGIT1 + 11, MARK_COUNT };

// Dwarf animations
enum { DWARF_IDLE = 0, DWARF_DIG };

// Run map icons (metatile order of assets/nodes.png)
enum { ICON_DIG = 0, ICON_VEIN, ICON_BLOCK, ICON_TUNNEL, ICON_LEDGER, ICON_NUGGET,
       ICON_CAMP, ICON_CORE, ICON_HINT, ICON_LIFE, ICON_RISKY, ICON_NODE_COUNT,
       // the shop goods follow, in enum ShopItem order (assets/nodes.png holds both)
       ICON_SHOP_FIRST = ICON_NODE_COUNT, ICON_COUNT = ICON_SHOP_FIRST + 9 };
// GBA button icons (metatile order of assets/buttons.png), drawn on the text layer
enum { BTN_A = 0, BTN_B, BTN_L, BTN_R, BTN_START, BTN_SELECT, BTN_DPAD, BTN_COUNT };
// Title menu icons (metatile order of assets/menu_icons.png)
enum { MICON_CONTINUE = 0, MICON_NEW, MICON_SHOP, MICON_RECORDS, MICON_OPTIONS, MICON_COUNT };

// Node palette banks on the map (they reuse region banks; rooms reset them)
enum { PAL_NODE_LIT = 1, PAL_NODE_DIM = 2, PAL_NODE_DONE = 3 };
// Canvas colours (BG2 pixel layer)
// (they live in the red text bank, after its ink colour, so the backdrop bank keeps 15 colours)
enum { CANVAS_LINE = 2, CANVAS_LINE_DIM = 3, CANVAS_LINE_LIT = 4, CANVAS_ALERT = 5 };

void render_init(void);        // mode 0 set-up; also restores it after render_title_picture()
void render_title_picture(void);   // mode 4: the full-screen title picture (assets/title.png)
void render_vblank(void);       // once per frame after vid_vsync(): OAM, animations
void render_clear(void);        // wipe every layer, hide sprites

// --- text (tile coordinates, 30x20) -----------------------------------------
void txt_puts(int tx, int ty, const char *s, int pal);
void txt_puts_center(int ty, const char *s, int pal);
void txt_putint(int tx, int ty, int v, int pal);      // left-aligned decimal
void txt_clear(void);
void txt_clear_rect(int tx, int ty, int w, int h);
int  txt_len(const char *s);

// --- puzzle grid --------------------------------------------------------------
// The grid origin is the tile position of cell (0,0); cells are 2x2 tiles.
void grid_set_origin(int tx, int ty);
// Cell size: 16 (default, metatiles + marks) or 8 (the core's picture: one
// tile per cell from assets/cells_small.png, a small cursor). Reset by render_clear().
void grid_set_cell_px(int px);
void grid_clear(void);
// tile: variant * 16 + bits. Variant 0 = rock, bits = thick edges (1 N, 2 E,
// 4 S, 8 W); variant 1 = dug gallery, bits = linked sides.
void grid_cell(int r, int c, int tile, int pal);
void grid_cell_pal(int r, int c, int pal);            // recolour without redrawing
void grid_mark(int r, int c, int mark);
void grid_cell_blank(int r, int c);                   // the cell (and its mark) vanish
void mark_at(int tx, int ty, int mark);               // a 16x16 mark at a tile position (text layer)
void region_palette(int bank, u16 fill);              // derives light/dark/edge

// --- run map --------------------------------------------------------------------------
void render_palettes_room(void);    // region banks for a puzzle room
void render_palettes_small_room(void);   // ...then the dark rock / grey note banks of a picture room
void render_canvas_on_top(bool on);      // canvas (BG2) above the cells: guide lines over a picture grid
void render_flash(bool on);              // the whole screen lit towards white (time pressure)
void render_set_biome(int biome);                  // backdrop tiles + palette, accent colour
void render_backdrop_scroll(int x, int y);         // BG3 offset (slow drift on the map)
void render_palettes_map(void);     // node state banks for the map
void map_icon(int tx, int ty, int icon, int pal);      // 2x2 metatile on the text layer
void button_icon(int tx, int ty, int button);          // 2x2 metatile on the text layer
// Solid box on the cell layer (hides the grid and everything below it)
void modal_fill(int tx, int ty, int w, int h);
void modal_clear(void);

// --- canvas: BG2 as a full-screen 4bpp pixel layer (lines of the run map) ------------
void canvas_clear(void);
void canvas_plot(int x, int y, int color);
void canvas_line(int x0, int y0, int x1, int y1, int color);
void canvas_rect(int x, int y, int w, int h, int color);   // filled
void canvas_show(bool on);

// --- sprites -----------------------------------------------------------------------
void cursor_set_cell(int r, int c, bool visible);
void cursor_set_px(int x, int y, bool visible);
void dwarf_set(int x, int y, bool visible);
void dwarf_play(int anim);
void dwarf_cosmetics(u8 mask);                     // shop.h COS_* bits
void merchant_set(int x, int y, bool visible);   // 64x64, plays its idle loop while visible     // 32x32 shopkeeper
void logo_show(bool visible);                      // title logo: eight affine sprites, centred near the top
void logo_set_scale(int scale256);                 // 256 = full size; pieces stay aligned at multiples of 1/4
void title_prompt(const char *s, int y, bool visible);   // text sprites on the title picture (NULL hides)
void button_sprite(int slot, int button, int x, int y, bool visible);   // 16x16 button icon sprite, slot 0..9
void button_sprites_clear(void);
void node_sprite(int icon, int x, int y, bool visible);   // a map node icon as a sprite
void menu_icon_set(int slot, int icon, int x, int y, bool lit, bool visible);   // slot 0..4; 64x64 box, 48 px art centred

// pixel position of a cell's top-left corner
int  grid_px_x(int c);
int  grid_px_y(int r);

#endif
