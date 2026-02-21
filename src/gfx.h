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

// 270 is how many tiles i think i need for six lines (240)/8 * (12*6)/8
// 12*6 is also a multiple of 8 so that's pretty neat as well.
#define GFX_TEXT_BMP_SIZE  270 // in tiles
#define GFX_TEXT_BMP_COLS  30  // tiles per column
#define GFX_TEXT_BMP_BLOCK 2
#define GFX_TEXT_BMP_VRAM  (&(tile_mem[GFX_TEXT_BMP_BLOCK][1]))

extern OBJ_ATTR gfx_oam_buffer[128];
extern int gfx_scroll_x;
extern int gfx_scroll_y;
extern uint gfx_map_width;
extern uint gfx_map_height;
extern const map_header_s *gfx_loaded_map;

extern u16 gfx_palette[16];

extern TILE gfx_text_bmp[GFX_TEXT_BMP_SIZE];

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

static inline void gfx_text_sync_row(int row) // copy row of tiles to VRAM
{
    // TODO: gfx_text_sync_row DMA copy?
    int ofs = row * GFX_TEXT_BMP_COLS;
    memcpy32(GFX_TEXT_BMP_VRAM + ofs, gfx_text_bmp + ofs,
             GFX_TEXT_BMP_COLS * sizeof(TILE) / 4);
}

#endif