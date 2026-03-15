#ifndef GFX_H
#define GFX_H

#define GFX_TEXT_BMP_COLS  30  // tiles per column
#define GFX_TEXT_BMP_ROWS  12 // in tiles; (12 * 8) / 8, where 12 is the size in
                              // pixels of each line
#define GFX_TEXT_BMP_SIZE  (GFX_TEXT_BMP_COLS * GFX_TEXT_BMP_ROWS) // in tiles
#define GFX_TEXT_BMP_BLOCK 2

#ifdef __ASSEMBLER__
#   define MEM_VRAM          0x06000000
#   define SIZEOF_TILE       0x20
#   define SIZEOF_CHARBLOCK  (SIZEOF_TILE * 512)
#   define GFX_TEXT_BMP_VRAM (MEM_VRAM                                         \
                             + SIZEOF_CHARBLOCK * GFX_TEXT_BMP_BLOCK           \
                             + SIZEOF_TILE * 1)

.extern gfx_text_bmp_buf
.extern gfx_text_bmp_dirty_rows

#else
#   define GFX_TEXT_BMP_VRAM (&(tile_mem[GFX_TEXT_BMP_BLOCK][1]))

#include <tonc_core.h>
#include "map_data.h"
#include "log.h"
#include "gba_util.h"

#define SCRW_T16 (SCREEN_WIDTH / 16)
#define SCRH_T16 (SCREEN_HEIGHT / 16)

#define GFX_OBJ_COUNT 128
#define GFX_BG0_INDEX 28 // ui layer
#define GFX_BG1_INDEX 29 // foreground/play layer
#define GFX_BG2_INDEX 30 // mountain parallax (for that one room)
#define GFX_BG3_INDEX 31 // sky background (for that one room)

#define GFX_CHAR_GAME_TILESET 0

typedef enum text_color
{
    TEXT_COLOR_WHITE,
    TEXT_COLOR_BLUE,
    TEXT_COLOR_YELLOW,
    TEXT_COLOR_BLACK,
}
text_color_e;

typedef enum gfx_pal
{
    GFX_PAL_BLACK,
    GFX_PAL_DARK_BLUE,
    GFX_PAL_DARK_PURPLE,
    GFX_PAL_DARK_GREEN,
    GFX_PAL_BROWN,
    GFX_PAL_DARK_GRAY,
    GFX_PAL_LIGHT_GRAY,
    GFX_PAL_WHITE,
    GFX_PAL_RED,
    GFX_PAL_ORANGE,
    GFX_PAL_YELLOW,
    GFX_PAL_GREEN,
    GFX_PAL_BLUE,
    GFX_PAL_INDIGO,
    GFX_PAL_PINK,
    GFX_PAL_PEACH
} gfx_pal_e;

typedef enum gfx_pal_mode
{
    GFX_PAL_MODE_NORMAL,
    GFX_PAL_MODE_LCD_CORRECTED,
}
gfx_pal_mode_e;

typedef enum gfx_bg_bpp
{
    GFX_BG_4BPP,
    GFX_BG_8BPP,
}
gfx_bg_bpp_e;

typedef struct gfx_frame
{
    u16 obj_pool_index;
    u8 width; // in tiles
    u8 height; // in tiles
    u8 frame_len;
    u8 obj_count;
} gfx_frame_s;

typedef struct gfx_sprite
{
    u8 frame_count;
    u8 loop; // bool
    u16 frame_pool_idx;
} gfx_sprite_s;

typedef struct gfx_obj {
    u16 a0; // contains only config for the sprite shape
    u16 a1; // contains only config for the sprite size
    u16 a2; // contains only config for the character index
    s8 ox;
    s8 oy;
    s8 flipped_ox;
    s8 flipped_oy;
} gfx_obj_s;

typedef struct gfx_sprdb
{
    const gfx_sprite_s *gfx_sprites;
    const gfx_frame_s *frame_pool;
    const gfx_obj_s *obj_pool;
}
gfx_sprdb_s;

typedef struct gfx_root_header {
    uintptr_t  frame_pool;
    uintptr_t  obj_pool;
    
    gfx_sprite_s sprite0;
} gfx_root_header_s;

typedef struct gfx_draw_sprite_state
{
    const gfx_sprdb_s *sprdb;

    OBJ_ATTR *dst_obj;
    uint dst_obj_count;

    u16 a0, a1, a2;
}
gfx_draw_sprite_state_s;

typedef struct gfx_bg
{
    u8 bpp;
    bool enabled;
    u8 priority;
    u8 char_block;

    u16 offset_x;
    u16 offset_y;

    const map_header_s *map;
    uint map_width;
    uint map_height;
}
gfx_bg_s;

extern gfx_bg_s gfx_bg[4];
extern OBJ_ATTR gfx_oam_buffer[GFX_OBJ_COUNT];
extern TILE gfx_text_bmp_buf[GFX_TEXT_BMP_SIZE];

extern u16 gfx_palette[16];

void gfx_init(void);
void gfx_new_frame(void);
void gfx_load_map(uint bg_idx, const map_header_s *map);
void gfx_mark_scroll_dirty(uint bg_idx);
INLINE void gfx_unload_map(uint bg_idx)
{
    gfx_bg[bg_idx].map = NULL;
    gfx_bg[bg_idx].map_width = 0;
    gfx_bg[bg_idx].map_height = 0;
}

void* gfx_alloc_cpybuf(size_t wsize);
bool gfx_queue_memcpy32(void *dst, const void *src, size_t wcount);
bool gfx_queue_memset32(void *dst, uint value, size_t wcount);

void gfx_set_palette_mode(gfx_pal_mode_e mode);
void gfx_reset_palette(void);
void gfx_set_palette_multiplied(FIXED factor);

// gfx_text_bmap_fill written in asm. For funsies.
void gfx_text_bmap_fill(uint oc, uint or_, uint cols, uint rows, u32 data[8]);
void gfx_text_bmap_print(uint x, uint y, const char *text, text_color_e color);
void gfx_text_bmap_dst_clear(uint row, uint row_count);
void gfx_text_bmap_dst_assign(uint row, uint row_count, uint src_row, uint pal);

static inline void gfx_text_bmap_clear(uint oc, uint or_, uint cols, uint rows)
{
    u32 bmap[8] = {0, 0, 0, 0, 0, 0, 0};
    gfx_text_bmap_fill(oc, or_, cols, rows, bmap);
}

static inline gfx_sprdb_s gfx_get_sprdb(const gfx_root_header_s *header)
{
    return (gfx_sprdb_s)
    {
        .gfx_sprites = (const gfx_sprite_s *)&header->sprite0,
        .frame_pool = (const gfx_frame_s *)((uintptr_t)header + header->frame_pool),
        .obj_pool = (const gfx_obj_s *)((uintptr_t)header + header->obj_pool)
    };
}

void gfx_draw_sprite(gfx_draw_sprite_state_s *state, uint spr_idx,
                     uint frame_idx, int draw_x, int draw_y);

#endif // !defined(__ASSEMBLER__)

#endif