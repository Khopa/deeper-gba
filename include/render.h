// Rendering: Mode 0, three regular backgrounds and a handful of sprites.
//   BG0  text + marks   charblock 0 (font, marks), screenblock 12, priority 0
//   BG1  grid cells     charblock 1 (cells, tiles 0..139), screenblock 13, priority 1
//   BG2  canvas         charblocks 2-3 (600 tiles), screenblock 14, priority 2
//   BG3  biome backdrop charblock 3 (tiles 96..511: the biome's block variants, 16/32/64 px), screenblock 15, priority 3
// (screenblocks 12..15 are the top of charblock 1, past the cell tiles)
//   OBJ  cursor (tiles 0-7), small cursor + chips (24-27), buttons (32-59), shared 64-639 (merchant / logo / foe / the dwarf's current frame), menu (640-959), icon slots (960-991), prompt (1004-1023)
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

// The player's dwarf (assets/dwarf.png: 64 px frames, the 48 px art centred):
// idle loop, walking loop, two stills. dwarf_set() places the 64 px box.
enum { DWARF_IDLE = 0, DWARF_WALK, DWARF_STAND, DWARF_BACK };
#define DWARF_BOX 64
#define DWARF_ART 48
#define DWARF_PAD 8

// The 16 px icons (enum Icon, ICON_*: build/gen/gfx_icons.h from assets/nodes/)
#include "gfx_icons.h"
// How an icon on the text layer is coloured: its own palette (loaded into the
// bank given), or the shared grey ramp of the far / passed map nodes
enum { ICON_LIT = 0, ICON_DIM, ICON_DONE };
// GBA button icons (metatile order of assets/buttons.png), drawn on the text layer
enum { BTN_A = 0, BTN_B, BTN_L, BTN_R, BTN_START, BTN_SELECT, BTN_DPAD, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_COUNT };
#define BTN_SPRITES 7                        // the first seven also exist as sprites (the modals)
// Title menu icons (metatile order of assets/menu_icons.png)
enum { MICON_CONTINUE = 0, MICON_NEW, MICON_SHOP, MICON_RECORDS, MICON_OPTIONS, MICON_COUNT };

// Grey banks of the map's far / passed icons (region banks; rooms reset them)
enum { PAL_ICON_DIM = 7, PAL_ICON_DONE = 8 };
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
// The two ores of a vein room drawn as gem icons (enum Icon) on region banks
// 6 and 7 instead of the light / dark ore marks; render_clear() forgets them
void render_set_gems(int icon_light, int icon_dark);
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
void render_palettes_icons(void);   // the grey banks (PAL_ICON_DIM / DONE) for the map's icons
// A 16 px icon on the text layer at a tile position. ICON_LIT loads the icon's
// palette into `bank` (a free region bank, 1..8) and draws with it; ICON_DIM /
// ICON_DONE draw the grey version on the shared grey banks (bank ignored).
void icon_at(int tx, int ty, int icon, int style, int bank);
void icon_clear(int tx, int ty);
// A 16 px icon sprite: slot 0..7, each slot with its own OBJ palette bank, so
// icons sit anywhere (a text row centred: y = 8 * row - 4)
void icon_sprite(int slot, int icon, int x, int y, bool visible);
void icon_sprites_clear(void);
// "<prefix><value>" then the icon (sprite `slot`) right after it, centred on
// the text row; returns the tile column after the icon
int  icon_label(int slot, int icon, int tx, int ty, const char *prefix, int value, int pal);
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
void dwarf_face(bool left);        // mirrored: the drawing faces right, flip it to look left
void dwarf_scale(int pct);         // 100 = as drawn; smaller shows the frame through affine matrix 3,
                                   // the art shrinking about the centre of its 64 px box (y must stay >= 0)
void merchant_set(int x, int y, bool visible);   // 64x64, plays its idle loop while visible
// A monster (enum Foe): its 9-frame idle loop shown 1.5x in a 128 px box at (x, y);
// it shares the merchant's tile region and loads when shown
void foe_show(int foe, int x, int y, bool visible);
void foe_hit(void);                                // flashes white and shakes for a few frames
void foe_lunge(void);                              // hops forward: it strikes
void render_flash_frames(int frames);              // render_flash(true) that turns itself off
// Rock chips (8x8 sprites, up to 8) and a cell-layer shake, for the wall mini-game
void chip_set(int slot, int x, int y, int kind, bool visible);
void chips_clear(void);
void render_cells_shift(int dx, int dy);           // scrolls the cell layer (0, 0 to rest)     // 32x32 shopkeeper
void logo_show(bool visible);                      // title logo: eight affine sprites, centred near the top
void logo_set_scale(int scale256);                 // 256 = full size; pieces stay aligned at multiples of 1/4
void title_prompt(const char *s, int y, bool visible);   // text sprites on the title picture (NULL hides)
void button_sprite(int slot, int button, int x, int y, bool visible);   // 16x16 button icon sprite, slot 0..9
void button_sprites_clear(void);
void menu_icon_set(int slot, int icon, int x, int y, bool lit, bool visible);   // slot 0..4; 64x64 box, 48 px art centred

// pixel position of a cell's top-left corner
int  grid_px_x(int c);
int  grid_px_y(int r);

#endif
