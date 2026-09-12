// Rendering: Mode 0, three regular backgrounds and a handful of sprites.
//   BG0  text + marks   charblock 0 (font, marks), screenblock 28, priority 0
//   BG1  grid cells     charblock 1 (cells),       screenblock 29, priority 1
//   BG2  backdrop       charblock 2,               screenblock 30, priority 2
//   OBJ  cursor (tiles 0-7), dwarf (tiles 8-23)
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
    PAL_BACKDROP  = 11,
    PAL_MARKS     = 12,
    PAL_TXT_GRAY  = 13,
    PAL_TXT_GOLD  = 14,
    PAL_TXT_RED   = 15,
};

// Overlay marks (metatile order of assets/marks.png)
enum { MARK_NONE = 0, MARK_CROSS, MARK_DIG, MARK_ALERT, MARK_GEM, MARK_ORE_LIGHT, MARK_ORE_DARK, MARK_COUNT };

// Dwarf animations
enum { DWARF_IDLE = 0, DWARF_DIG };

// Run map icons (metatile order of assets/nodes.png)
enum { ICON_DIG = 0, ICON_VEIN, ICON_BLOCK, ICON_TUNNEL, ICON_LEDGER, ICON_NUGGET,
       ICON_CAMP, ICON_CORE, ICON_HINT, ICON_LIFE, ICON_RISKY, ICON_COUNT };
// Node palette banks on the map (they reuse region banks; rooms reset them)
enum { PAL_NODE_LIT = 1, PAL_NODE_DIM = 2, PAL_NODE_DONE = 3 };
// Canvas colours (BG2 pixel layer)
enum { CANVAS_LINE = 1, CANVAS_LINE_DIM = 2, CANVAS_LINE_LIT = 3 };

void render_init(void);
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
void grid_clear(void);
void grid_cell(int r, int c, int edges, int pal);     // edges: 1 N, 2 E, 4 S, 8 W
void grid_cell_pal(int r, int c, int pal);            // recolour without redrawing
void grid_mark(int r, int c, int mark);
void region_palette(int bank, u16 fill);              // derives light/dark/edge

// --- run map --------------------------------------------------------------------------
void render_palettes_room(void);    // region banks for a puzzle room
void render_palettes_map(void);     // node state banks for the map
void map_icon(int tx, int ty, int icon, int pal);      // 2x2 metatile on the text layer

// --- canvas: BG2 as a full-screen 4bpp pixel layer (lines of the run map) ------------
void canvas_clear(void);
void canvas_plot(int x, int y, int color);
void canvas_line(int x0, int y0, int x1, int y1, int color);
void canvas_show(bool on);

// --- sprites -----------------------------------------------------------------------
void cursor_set_cell(int r, int c, bool visible);
void cursor_set_px(int x, int y, bool visible);
void dwarf_set(int x, int y, bool visible);
void dwarf_play(int anim);

// pixel position of a cell's top-left corner
int  grid_px_x(int c);
int  grid_px_y(int r);

#endif
