// See render.h for the layer layout.
#include <string.h>
#include "render.h"
#include "gfx_font.h"
#include "gfx_cells.h"
#include "gfx_marks.h"
#include "gfx_cursor.h"
#include "gfx_cursor_small.h"
#include "gfx_cells_small.h"
#include "gfx_dwarf.h"
#include "gfx_nodes.h"
#include "gfx_merchant.h"
#include "gfx_logo.h"
#include "gfx_menu_icons.h"
#include "gfx_title.h"
#include "gfx_buttons.h"
#include "gfx_back_earth.h"
#include "gfx_back_rock.h"
#include "gfx_back_ice.h"
#include "gfx_back_lava.h"
#include "gfx_back_crystal.h"
#include "gfx_back_core.h"
#include "biome.h"

// Must match FONT_CHARS in tools/make_assets.py
static const char FONT_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?:.-/%><#',\x01\x02\x03\x04\x05\x06+=*()";

#define CBB_TEXT    0
#define CBB_CELLS   1
#define CBB_BACK    2
// Screen maps live at the end of charblock 1 (cells use tiles 0..139 of it),
// so charblock 3 stays whole for the canvas spill and the biome variants:
// screenblocks 28..31 would sit on tiles 256..511 of charblock 3.
#define SBB_TEXT    12
#define SBB_CELLS   13
#define SBB_BACK    14
#define CBB_BIOME   3
#define SBB_BIOME   15
#define BIOME_TILE_BASE 96                    // above the canvas spill (tiles 0-87 of charblock 3)
#define BIOME_TILE_ROOM (512 - BIOME_TILE_BASE)   // tiles 96..511 of charblock 3 for the loaded variants
#define BIOME_FLAT_TILE (BIOME_TILE_BASE - 1)     // a solid tile in the sheet's main colour: the side margins
#define PAL_CANVAS      PAL_TXT_RED           // canvas line colours sit after the red ink
#define BACKDROP_FADE   9                     // BG3 brightness cut, 0..16

#define MARK_TILE_BASE  fontTileCount        // marks follow the font in charblock 0
#define NODE_TILE_BASE  (MARK_TILE_BASE + marksTileCount)
#define BUTTON_TILE_BASE (NODE_TILE_BASE + nodesTileCount)
#define MODAL_TILE      1                    // a solid tile in the cells block (colour 2 of the gray text bank)
#define CANVAS_TILES    (TILES_W * TILES_H)  // BG2 canvas: one tile per screen tile, spilling into charblock 3
#define CELL_TILE_BASE  4                    // tiles 0-3 of the cells block stay blank
#define SMALL_TILE_BASE (CELL_TILE_BASE + cellsTileCount)   // 8 px cells after the metatiles

#define OBJ_CURSOR  0                        // OAM slots
#define OBJ_DWARF   1
#define OBJ_MERCHANT 2
#define OBJ_MENU     4                       // 5 slots
#define OBJ_LOGO     9                       // 2 slots
#define OBJ_LAST     11
#define OBJ_TILE_CURSOR 0                    // obj tile indices (4 per 16x16 frame)
#define OBJ_TILE_DWARF  8
#define OBJ_TILE_MERCHANT 256                // 9 frames of 64 tiles (64x64), tiles 256..831
#define MERCHANT_FRAMES   (merchantTileCount / 64)
#define MERCHANT_FRAME_LEN 8                 // game frames per animation frame
#define OBJ_TILE_MENU     56                 // 5 icons of 16 tiles
#define OBJ_TILE_LOGO     136                // 2 halves of 32 tiles
#define OBJ_TILE_CURSOR_SMALL 200            // 2 frames of 1 tile

static OBJ_ATTR obj_buffer[128];
static u32 frame;
static int grid_tx = 1, grid_ty = 3;
static int cell_px = 16;
static int backdrop_margin;                  // px left blank on each side to centre the blocks
static int dwarf_anim, dwarf_x, dwarf_y;
static bool dwarf_visible, merchant_visible;

// per biome: the tile data, its variants (metatiles) and the tiles per variant (4, 16 or 64)
#define BACKDROP(name) { name##Tiles, name##MetaCount, name##TileCount / name##MetaCount, name##Pal }
static const struct { const unsigned int *tiles; int variants, per_variant; const unsigned short *pal; } backdrops[BIOME_COUNT] = {
    BACKDROP(back_earth), BACKDROP(back_rock), BACKDROP(back_ice), BACKDROP(back_lava), BACKDROP(back_crystal), BACKDROP(back_core),
};

// --- colours ---------------------------------------------------------------------
#define CLR(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))
#define C_BACKDROP CLR(3, 2, 2)
#define C_WHITE    CLR(31, 31, 30)
#define C_MODAL    CLR(4, 4, 7)
#define C_GRAY     CLR(17, 16, 15)
#define C_GOLD     CLR(30, 25, 8)
#define C_RED      CLR(29, 7, 6)
#define C_INK      CLR(4, 3, 2)
#define C_LIGHTINK CLR(30, 30, 28)

// Eight rock tones, chosen to stay distinct on a GBA screen.
static const u16 region_fills[8] = {
    CLR(25, 21, 13),      // sandstone
    CLR(23, 12, 10),      // red clay
    CLR(13, 16, 20),      // slate
    CLR(13, 18, 11),      // mossy stone
    CLR(18, 13, 21),      // amethyst
    CLR(15, 11, 7),       // dark earth
    CLR(11, 20, 18),      // teal ore
    CLR(22, 22, 22),      // pale granite
};

static u16 shade(u16 c, int d)
{
    int r = clampi((c & 31) + d, 0, 31);
    int g = clampi(((c >> 5) & 31) + d, 0, 31);
    int b = clampi(((c >> 10) & 31) + d, 0, 31);
    return CLR(r, g, b);
}

void region_palette(int bank, u16 fill)
{
    pal_bg_bank[bank][1] = fill;
    pal_bg_bank[bank][2] = shade(fill, 4);
    pal_bg_bank[bank][3] = shade(fill, -5);
    pal_bg_bank[bank][4] = C_INK;
}

static void set_text_pal(int bank, u16 color) { pal_bg_bank[bank][1] = color; }

void render_init(void)
{
    REG_DISPCNT = 0;

    memcpy32(&tile_mem[CBB_TEXT][0], fontTiles, fontTilesLen / 4);
    memcpy32(&tile_mem[CBB_TEXT][MARK_TILE_BASE], marksTiles, marksTilesLen / 4);
    memset32(&tile_mem[CBB_CELLS][0], 0, CELL_TILE_BASE * 8);
    memcpy32(&tile_mem[CBB_CELLS][CELL_TILE_BASE], cellsTiles, cellsTilesLen / 4);
    memcpy32(&tile_mem[CBB_TEXT][NODE_TILE_BASE], nodesTiles, nodesTilesLen / 4);
    canvas_clear();
    memcpy32(&tile_mem_obj[0][OBJ_TILE_CURSOR], cursorTiles, cursorTilesLen / 4);
    memcpy32(&tile_mem_obj[0][OBJ_TILE_CURSOR_SMALL], cursor_smallTiles, cursor_smallTilesLen / 4);
    memcpy32(&tile_mem[CBB_CELLS][SMALL_TILE_BASE], cells_smallTiles, cells_smallTilesLen / 4);
    memcpy32(&tile_mem_obj[0][OBJ_TILE_DWARF], dwarfTiles, dwarfTilesLen / 4);
    memcpy32(&tile_mem_obj[0][OBJ_TILE_MERCHANT], merchantTiles, merchantTilesLen / 4);
    memcpy32(&tile_mem_obj[0][OBJ_TILE_MENU], menu_iconsTiles, menu_iconsTilesLen / 4);
    memcpy32(&tile_mem_obj[0][OBJ_TILE_LOGO], logoTiles, logoTilesLen / 4);
    memcpy32(&tile_mem[CBB_TEXT][BUTTON_TILE_BASE], buttonsTiles, buttonsTilesLen / 4);
    memset32(&tile_mem[CBB_CELLS][MODAL_TILE], 0x22222222, 8);

    pal_bg_mem[0] = C_BACKDROP;
    set_text_pal(PAL_TXT_WHITE, C_WHITE);
    set_text_pal(PAL_TXT_GRAY, C_GRAY);
    set_text_pal(PAL_TXT_GOLD, C_GOLD);
    set_text_pal(PAL_TXT_RED, C_RED);
    render_palettes_room();
    region_palette(PAL_CELL_CONFLICT, CLR(26, 8, 6));
    region_palette(PAL_CELL_HILITE, CLR(28, 26, 14));
    pal_bg_bank[PAL_CANVAS][CANVAS_LINE] = CLR(12, 10, 8);
    pal_bg_bank[PAL_CANVAS][CANVAS_LINE_DIM] = CLR(6, 5, 4);
    pal_bg_bank[PAL_CANVAS][CANVAS_LINE_LIT] = C_GOLD;
    memcpy16(pal_bg_bank[PAL_MARKS], marksPal, 16);    // imported marks use indices 5..15
    pal_bg_bank[PAL_MARKS][1] = C_INK;                  // drawn marks: fixed ink colours
    pal_bg_bank[PAL_MARKS][2] = C_LIGHTINK;
    pal_bg_bank[PAL_MARKS][3] = C_RED;
    pal_bg_bank[PAL_MARKS][4] = C_GOLD;
    for (int i = 2; i < 16; i++) pal_bg_bank[PAL_TXT_WHITE][i] = buttonsPal[i];   // button icons share the white text bank
    pal_bg_bank[PAL_TXT_GRAY][2] = C_MODAL;

    memcpy16(pal_obj_bank[1], dwarfPal, 16);
    memcpy16(pal_obj_bank[3], merchantPal, 16);
    memcpy16(pal_obj_bank[4], menu_iconsPal, 16);
    memcpy16(pal_obj_bank[5], logoPal, 16);
    for (int i = 1; i < 16; i++) {                      // dimmed copy of the icon palette
        u16 c = menu_iconsPal[i];
        int lum = ((c & 31) + ((c >> 5) & 31) + ((c >> 10) & 31)) / 3;
        pal_obj_bank[6][i] = CLR(lum / 2 + 3, lum / 2 + 3, lum / 2 + 4);
    }
    pal_obj_bank[0][1] = C_WHITE;
    pal_obj_bank[2][1] = C_GOLD;

    REG_BG0CNT = BG_CBB(CBB_TEXT)  | BG_SBB(SBB_TEXT)  | BG_4BPP | BG_REG_32x32 | BG_PRIO(0);
    REG_BG1CNT = BG_CBB(CBB_CELLS) | BG_SBB(SBB_CELLS) | BG_4BPP | BG_REG_32x32 | BG_PRIO(1);
    REG_BG2CNT = BG_CBB(CBB_BACK)  | BG_SBB(SBB_BACK)  | BG_4BPP | BG_REG_32x32 | BG_PRIO(2);
    REG_BG3CNT = BG_CBB(CBB_BIOME) | BG_SBB(SBB_BIOME) | BG_4BPP | BG_REG_32x32 | BG_PRIO(3);
    render_set_biome(BIOME_EARTH);

    oam_init(obj_buffer, 128);
    render_clear();

    REG_BLDCNT = BLD_BUILD(BLD_BG3, 0, 3);   // BG3 alone fades towards black: readable text on detailed backdrops
    REG_BLDY = BACKDROP_FADE;
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_BG2 | DCNT_BG3 | DCNT_OBJ | DCNT_OBJ_1D;
}

// The title picture takes the screen in mode 4 (8bpp bitmap at the start of VRAM,
// 256-colour palette); it overwrites the tile blocks and the palettes, so the
// caller goes back through render_init() before drawing anything else.
void render_title_picture(void)
{
    REG_DISPCNT = 0;
    oam_init(obj_buffer, 128);
    oam_copy(oam_mem, obj_buffer, 128);
    memcpy32(vid_mem, titleBitmap, titleBitmapLen / 4);
    memcpy16(pal_bg_mem, titlePal, 256);
    REG_BG_AFFINE[2].pa = 256; REG_BG_AFFINE[2].pb = 0;
    REG_BG_AFFINE[2].pc = 0;   REG_BG_AFFINE[2].pd = 256;
    REG_BG_AFFINE[2].dx = 0;   REG_BG_AFFINE[2].dy = 0;
    REG_BLDCNT = 0;
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2;
}

void render_vblank(void)
{
    frame++;
    // cursor: two-frame pulse; dwarf: two-frame animation
    int cframe = (frame >> 4) & 1;
    obj_buffer[OBJ_CURSOR].attr2 = ATTR2_PALBANK(0) |
        ATTR2_ID(cell_px == 8 ? OBJ_TILE_CURSOR_SMALL + cframe : OBJ_TILE_CURSOR + cframe * 4);
    if (dwarf_visible) {
        int dframe = dwarf_anim == DWARF_DIG ? ((frame >> 3) & 1) : ((frame >> 5) & 1);
        obj_buffer[OBJ_DWARF].attr2 = ATTR2_PALBANK(1) | ATTR2_ID(OBJ_TILE_DWARF + (dwarf_anim * 2 + dframe) * 4);
    }
    if (merchant_visible) {                       // the merchant's idle loop (assets/merchant.png)
        int mframe = (int)((frame / MERCHANT_FRAME_LEN) % MERCHANT_FRAMES);
        obj_buffer[OBJ_MERCHANT].attr2 = ATTR2_PALBANK(3) | ATTR2_ID(OBJ_TILE_MERCHANT + mframe * 64);
    }
    oam_copy(oam_mem, obj_buffer, OBJ_LAST);
}

void render_clear(void)
{
    txt_clear();
    grid_clear();
    canvas_show(false);
    grid_set_cell_px(16);
    render_canvas_on_top(false);
    cursor_set_px(0, 0, false);
    dwarf_set(0, 0, false);
    merchant_set(0, 0, false);
    logo_set(0, 0, false);
    for (int i = 0; i < MICON_COUNT; i++) menu_icon_set(i, 0, 0, 0, false, false);
    render_backdrop_scroll(0, 0);
}

// --- text ---------------------------------------------------------------------------

static int font_index(char c)
{
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    for (int i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return i;
    return 0;
}

int txt_len(const char *s) { return (int)strlen(s); }

void txt_puts(int tx, int ty, const char *s, int pal)
{
    if (ty < 0 || ty >= 32) return;
    u16 *row = &se_mem[SBB_TEXT][ty * 32];
    for (; *s && tx < 32; s++, tx++)
        if (tx >= 0) row[tx] = (u16)(SE_PALBANK(pal) | font_index(*s));
}

void txt_puts_center(int ty, const char *s, int pal)
{
    txt_puts((TILES_W - txt_len(s)) / 2, ty, s, pal);
}

void txt_putint(int tx, int ty, int v, int pal)
{
    char buf[12];
    int i = 11;
    buf[i] = 0;
    bool neg = v < 0;
    unsigned u = neg ? (unsigned)(-v) : (unsigned)v;
    do { buf[--i] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) buf[--i] = '-';
    txt_puts(tx, ty, buf + i, pal);
}

void txt_clear(void) { memset16(&se_mem[SBB_TEXT][0], 0, 32 * 32); }

void txt_clear_rect(int tx, int ty, int w, int h)
{
    for (int y = ty; y < ty + h && y < 32; y++)
        for (int x = tx; x < tx + w && x < 32; x++)
            se_mem[SBB_TEXT][y * 32 + x] = 0;
}

// --- grid -------------------------------------------------------------------------------

void grid_set_origin(int tx, int ty) { grid_tx = tx; grid_ty = ty; }

void grid_set_cell_px(int px) { cell_px = px == 8 ? 8 : 16; }

void grid_clear(void) { memset16(&se_mem[SBB_CELLS][0], 0, 32 * 32); }

int grid_px_x(int c) { return (grid_tx + (cell_px == 8 ? c : 2 * c)) * 8; }
int grid_px_y(int r) { return (grid_ty + (cell_px == 8 ? r : 2 * r)) * 8; }

static void put_meta(int sbb, int tx, int ty, int tile, int pal)
{
    u16 *m = &se_mem[sbb][ty * 32 + tx];
    u16 base = (u16)(SE_PALBANK(pal) | tile);
    m[0] = base;
    m[1] = base + 1;
    m[32] = base + 2;
    m[33] = base + 3;
}

void grid_cell(int r, int c, int tile, int pal)
{
    if (cell_px == 8) {
        se_mem[SBB_CELLS][(grid_ty + r) * 32 + grid_tx + c] = (u16)(SE_PALBANK(pal) | (SMALL_TILE_BASE + (tile & 7)));
        return;
    }
    put_meta(SBB_CELLS, grid_tx + 2 * c, grid_ty + 2 * r, CELL_TILE_BASE + (tile & 31) * 4, pal);
}

void grid_cell_pal(int r, int c, int pal)
{
    if (cell_px == 8) {
        u16 *e = &se_mem[SBB_CELLS][(grid_ty + r) * 32 + grid_tx + c];
        *e = (u16)((*e & ~SE_PALBANK_MASK) | SE_PALBANK(pal));
        return;
    }
    u16 *m = &se_mem[SBB_CELLS][(grid_ty + 2 * r) * 32 + grid_tx + 2 * c];
    for (int k = 0; k < 4; k++) {
        u16 *e = &m[(k >> 1) * 32 + (k & 1)];
        *e = (u16)((*e & ~SE_PALBANK_MASK) | SE_PALBANK(pal));
    }
}

// Small cells have no marks; a burst on one is drawn 16 px wide from the cell's
// corner (it spills over the neighbours for its few frames, then is cleared).
void grid_mark(int r, int c, int mark)
{
    if (cell_px == 8) {
        put_meta(SBB_TEXT, grid_tx + c, grid_ty + r, MARK_TILE_BASE + mark * 4, PAL_MARKS);
        return;
    }
    put_meta(SBB_TEXT, grid_tx + 2 * c, grid_ty + 2 * r, MARK_TILE_BASE + mark * 4, PAL_MARKS);
}

// --- run map ----------------------------------------------------------------------------------

void render_set_biome(int biome)
{
    const BiomeInfo *bi = biome_info(biome);
    biome = clampi(biome, 0, BIOME_COUNT - 1);
    pal_bg_mem[0] = bi->backdrop;
    pal_bg_bank[PAL_TXT_GOLD][1] = bi->accent;
    pal_bg_bank[PAL_CANVAS][CANVAS_LINE_LIT] = bi->accent;
    for (int i = 1; i < 16; i++) pal_bg_bank[PAL_BACKDROP][i] = backdrops[biome].pal[i];

    // The biome's sheet holds up to 16 variants of a square block (16, 32 or
    // 64 px); as many as fit the free tiles go to VRAM, drawn at random, and
    // every block of the screen map picks one of those at random too, so no
    // two screens tile the rock the same way.
    int per = backdrops[biome].per_variant;           // tiles per variant: 4, 16 or 64
    int side = per >= 64 ? 8 : per >= 16 ? 4 : 2;     // block side in tiles
    int variants = backdrops[biome].variants;
    if (variants < 1) variants = 1;
    if (variants > 16) variants = 16;
    u32 seed = frame * 2654435761u + (u32)biome * 40503u + 1u;
    int order[16];
    for (int i = 0; i < 16; i++) order[i] = i % variants;
    for (int i = variants - 1; i > 0; i--) {          // shuffle the variant list
        seed = seed * 1664525u + 1013904223u;
        int j = (int)((seed >> 16) % (u32)(i + 1));
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }
    int loaded = variants;
    if (loaded > BIOME_TILE_ROOM / per) loaded = BIOME_TILE_ROOM / per;
    for (int k = 0; k < loaded; k++)
        memcpy32(&tile_mem[CBB_BIOME][BIOME_TILE_BASE + k * per],
                 backdrops[biome].tiles + order[k] * per * 8, per * 8);
    // Blocks that do not divide the 240 px width are centred: whole columns
    // only, the rest of the screen on each side is a flat tile in the sheet's
    // main colour (index 1), and the layer is scrolled by the margin.
    int block_px = side * 8, cols = SCREEN_WIDTH / block_px;
    backdrop_margin = (SCREEN_WIDTH - cols * block_px) / 2;
    memset32(&tile_mem[CBB_BIOME][BIOME_FLAT_TILE], 0x11111111, 8);
    for (int by = 0; by < 32; by += side)
        for (int bx = 0; bx < 32; bx += side) {
            seed = seed * 1664525u + 1013904223u;
            int k = (int)((seed >> 16) % (u32)loaded);
            bool margin = backdrop_margin && bx >= cols * side;
            for (int ty = 0; ty < side; ty++)
                for (int tx = 0; tx < side; tx++)
                    se_mem[SBB_BIOME][(by + ty) * 32 + bx + tx] = margin
                        ? (u16)(SE_PALBANK(PAL_BACKDROP) | BIOME_FLAT_TILE)
                        : (u16)(SE_PALBANK(PAL_BACKDROP) | (BIOME_TILE_BASE + k * per + ty * side + tx));
        }
    render_backdrop_scroll(0, 0);
}

void render_backdrop_scroll(int x, int y)
{
    REG_BG3HOFS = (u16)(x - backdrop_margin);
    REG_BG3VOFS = (u16)y;
}

void render_palettes_room(void)
{
    for (int i = 0; i < 8; i++) region_palette(PAL_REGION0 + i, region_fills[i]);
}

void render_palettes_small_room(void)
{
    region_palette(PAL_REGION0, CLR(10, 8, 7));           // unknown cells: dark rock
    region_palette(PAL_REGION0 + 6, CLR(17, 16, 16));     // rock notes: grey, ink cross
}

void render_canvas_on_top(bool on)
{
    REG_BG1CNT = BG_CBB(CBB_CELLS) | BG_SBB(SBB_CELLS) | BG_4BPP | BG_REG_32x32 | BG_PRIO(on ? 2 : 1);
    REG_BG2CNT = BG_CBB(CBB_BACK)  | BG_SBB(SBB_BACK)  | BG_4BPP | BG_REG_32x32 | BG_PRIO(on ? 1 : 2);
}

void render_palettes_map(void)
{
    memcpy16(pal_bg_bank[PAL_NODE_LIT], nodesPal, 16);
    for (int i = 1; i < 16; i++) {
        u16 c = nodesPal[i];
        int lum = ((c & 31) + ((c >> 5) & 31) + ((c >> 10) & 31)) / 3;
        pal_bg_bank[PAL_NODE_DIM][i] = CLR(lum / 2 + 4, lum / 2 + 4, lum / 2 + 4);
        pal_bg_bank[PAL_NODE_DONE][i] = CLR(lum / 3 + 2, lum / 3 + 2, lum / 3 + 2);
    }
}

void map_icon(int tx, int ty, int icon, int pal)
{
    put_meta(SBB_TEXT, tx, ty, NODE_TILE_BASE + icon * 4, pal);
}

void button_icon(int tx, int ty, int button)
{
    put_meta(SBB_TEXT, tx, ty, BUTTON_TILE_BASE + button * 4, PAL_TXT_WHITE);
}

void modal_fill(int tx, int ty, int w, int h)
{
    for (int y = ty; y < ty + h && y < 32; y++)
        for (int x = tx; x < tx + w && x < 32; x++)
            se_mem[SBB_CELLS][y * 32 + x] = (u16)(SE_PALBANK(PAL_TXT_GRAY) | MODAL_TILE);
}

void modal_clear(void) { grid_clear(); }

// --- canvas ----------------------------------------------------------------------------------------

void canvas_clear(void)
{
    memset32(&tile_mem[CBB_BACK][0], 0, CANVAS_TILES * 8);
}

void canvas_plot(int x, int y, int color)
{
    if ((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return;
    int tile = (y >> 3) * TILES_W + (x >> 3);
    u32 *row = &tile_mem[CBB_BACK][tile].data[y & 7];
    int shift = (x & 7) * 4;
    *row = (*row & ~(0xFu << shift)) | ((u32)color << shift);
}

void canvas_line(int x0, int y0, int x1, int y1, int color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;   // dy <= 0
    int err = dx + dy;
    for (;;) {
        canvas_plot(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void canvas_show(bool on)
{
    if (!on) { memset16(&se_mem[SBB_BACK][0], 0, 32 * 32); return; }
    for (int ty = 0; ty < TILES_H; ty++)
        for (int tx = 0; tx < TILES_W; tx++)
            se_mem[SBB_BACK][ty * 32 + tx] = (u16)(SE_PALBANK(PAL_CANVAS) | (ty * TILES_W + tx));
}

// --- sprites ------------------------------------------------------------------------------

void cursor_set_px(int x, int y, bool visible)
{
    OBJ_ATTR *o = &obj_buffer[OBJ_CURSOR];
    if (!visible) { obj_hide(o); return; }
    if (cell_px == 8)
        obj_set_attr(o, ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(y), ATTR1_SIZE_8 | ATTR1_X(x),
                     ATTR2_PALBANK(0) | ATTR2_ID(OBJ_TILE_CURSOR_SMALL));
    else
        obj_set_attr(o, ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(y), ATTR1_SIZE_16 | ATTR1_X(x),
                     ATTR2_PALBANK(0) | ATTR2_ID(OBJ_TILE_CURSOR));
}

void cursor_set_cell(int r, int c, bool visible)
{
    cursor_set_px(grid_px_x(c), grid_px_y(r), visible);
}

void dwarf_set(int x, int y, bool visible)
{
    dwarf_x = x;
    dwarf_y = y;
    dwarf_visible = visible;
    OBJ_ATTR *o = &obj_buffer[OBJ_DWARF];
    if (!visible) { obj_hide(o); return; }
    obj_set_attr(o, ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(y), ATTR1_SIZE_16 | ATTR1_X(x),
                 ATTR2_PALBANK(1) | ATTR2_ID(OBJ_TILE_DWARF));
}

void dwarf_play(int anim) { dwarf_anim = anim; }

void dwarf_cosmetics(u8 mask)
{
    memcpy16(pal_obj_bank[1], dwarfPal, 16);
    if (mask & 1) pal_obj_bank[1][5] = CLR(30, 24, 6);     // golden helmet
    if (mask & 2) pal_obj_bank[1][3] = CLR(24, 8, 4);      // red beard
}

void logo_set(int x, int y, bool visible)
{
    for (int half = 0; half < 2; half++) {
        OBJ_ATTR *o = &obj_buffer[OBJ_LOGO + half];
        if (!visible) { obj_hide(o); continue; }
        obj_set_attr(o, ATTR0_WIDE | ATTR0_4BPP | ATTR0_Y(y), ATTR1_SIZE_64x32 | ATTR1_X(x + 64 * half),
                     ATTR2_PALBANK(5) | ATTR2_ID(OBJ_TILE_LOGO + 32 * half));
    }
}

void menu_icon_set(int slot, int icon, int x, int y, bool lit, bool visible)
{
    if (slot < 0 || slot >= MICON_COUNT) return;
    OBJ_ATTR *o = &obj_buffer[OBJ_MENU + slot];
    if (!visible) { obj_hide(o); return; }
    if (icon < 0 || icon >= MICON_COUNT) icon = 0;
    obj_set_attr(o, ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(y), ATTR1_SIZE_32 | ATTR1_X(x),
                 ATTR2_PALBANK(lit ? 4 : 6) | ATTR2_ID(OBJ_TILE_MENU + 16 * icon));
}

void merchant_set(int x, int y, bool visible)
{
    merchant_visible = visible;
    OBJ_ATTR *o = &obj_buffer[OBJ_MERCHANT];
    if (!visible) { obj_hide(o); return; }
    obj_set_attr(o, ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(y), ATTR1_SIZE_64 | ATTR1_X(x),
                 ATTR2_PALBANK(3) | ATTR2_ID(OBJ_TILE_MERCHANT));
}
