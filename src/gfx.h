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

#define GFX_CHAR_GAME_TILESET 128

// end: 1791 (0x6ff)
// 450 is how many tiles i think i need for five lines (240x12*5)/32
// 12*5 is also a multiple of 8 so that's pretty neat as well.
#define GFX_TEXT_BMP_SIZE 450 // in tiles
#define GFX_TEXT_BMP_COLS 240 // pixels per column
#define GFX_TEXT_BMP ((&(tile_mem[0][0]))) // ((tile_mem + (0x400 - GFX_TEXT_BMP_SIZE)))

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
INLINE void gfx_unload_map(void)
{
    gfx_loaded_map = NULL;
    gfx_map_width = 0;
    gfx_map_height = 0;
}

void gfx_reset_palette(void);
void gfx_set_palette_multiplied(FIXED factor);

void gfx_text_bmap_print(int x, int y, const char *text);

// not really a blit because it ORs rather than sets but good enough!
static inline void gfx_text_bmap_blit_pixel(int x, int y, int pixel)
{
    // LOG_DBG("(%i, %i) -> tile %i row %i shift %i", x, y, y & 7, (y / 8) * GFX_TEXT_BMP_COLS + (x / 8), ((x & 7) * 4));

    // divide all by 8?
    // WHY TF IS THIS NOT WORKING?? IT ONLY WORKS WHEN x (and presumably y) IS ZERO??
    TILE *tile = GFX_TEXT_BMP + ((y / 8) * GFX_TEXT_BMP_COLS + (x / 8));
    tile->data[y & 7] |= (pixel & 0xF) << ((x & 7) * 4);
}

#endif