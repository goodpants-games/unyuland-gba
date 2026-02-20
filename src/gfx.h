#ifndef GFX_H
#define GFX_H

#include <tonc_core.h>
#include "map_data.h"

#define SCRW_T16 (SCREEN_WIDTH / 16)
#define SCRH_T16 (SCREEN_HEIGHT / 16)

#define GFX_UI_INDEX  28
#define GFX_BG0_INDEX 29 // foreground/play layer
#define GFX_BG1_INDEX 30 // mountain parallax (for that one room)
#define GFX_BG2_INDEX 31 // sky background (for that one room)

#define GFX_CHAR_GAME_TILESET 128

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

#endif