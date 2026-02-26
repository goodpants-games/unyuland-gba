#ifndef GFX_H
#define GFX_H

#include <tonc_core.h>
#include "map_data.h"
#include "log.h"

#define SCRW_T16 (SCREEN_WIDTH / 16)
#define SCRH_T16 (SCREEN_HEIGHT / 16)

#define GFX_BG0_INDEX 28 // ui layer
#define GFX_BG1_INDEX 29 // foreground/play layer
#define GFX_BG2_INDEX 30 // mountain parallax (for that one room)
#define GFX_BG3_INDEX 31 // sky background (for that one room)

#define GFX_CHAR_GAME_TILESET 0

#define GFX_TEXT_BMP_COLS  30  // tiles per column
#define GFX_TEXT_BMP_ROWS  12 // in tiles; (12 * 8) / 8, where 12 is the size in
                              // pixels of each line
#define GFX_TEXT_BMP_SIZE  (GFX_TEXT_BMP_COLS * GFX_TEXT_BMP_ROWS) // in tiles
#define GFX_TEXT_BMP_BLOCK 2
#define GFX_TEXT_BMP_VRAM  (&(tile_mem[GFX_TEXT_BMP_BLOCK][1]))

typedef enum text_color
{
    TEXT_COLOR_WHITE,
    TEXT_COLOR_BLUE,
    TEXT_COLOR_YELLOW,
    TEXT_COLOR_BLACK,
}
text_color_e;

extern OBJ_ATTR gfx_oam_buffer[128];
extern int gfx_scroll_x;
extern int gfx_scroll_y;
extern uint gfx_map_width;
extern uint gfx_map_height;
extern const map_header_s *gfx_loaded_map;

extern u16 gfx_palette[16];

void gfx_init(void);
void gfx_new_frame(void);
void gfx_load_map(const map_header_s *map);
void gfx_mark_scroll_dirty(void);
INLINE void gfx_unload_map(void)
{
    gfx_loaded_map = NULL;
    gfx_map_width = 0;
    gfx_map_height = 0;
}

void gfx_reset_palette(void);
void gfx_set_palette_multiplied(FIXED factor);

void gfx_text_bmap_fill(int oc, int or_, int cols, int rows, u32 data[8]);
void gfx_text_bmap_print(int x, int y, const char *text, text_color_e color);
void gfx_text_bmap_dst_clear(int row, int row_count);
void gfx_text_bmap_dst_assign(int row, int row_count, int src_row);

#endif