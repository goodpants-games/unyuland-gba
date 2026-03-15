#include <tonc.h>
#include <ctype.h>
#include <font_gfx.h>
#include "gfx.h"
#include "gba_util.h"
#include <string.h>
#include <stdalign.h>










//------------------------------------------------------------------------------
// defs
//------------------------------------------------------------------------------
#pragma region defs

#define TEXT_CHAR_ID(i) ((i) * 4)
#define DEFER_QUEUE_MAX_SIZE 8
#define TEXT_QUEUE_MAX_SIZE 4096

typedef enum text_queue_action
{
    TEXT_QUEUE_BLIT,
    TEXT_QUEUE_BLIT_COLORED,
    TEXT_QUEUE_FILL,
    TEXT_QUEUE_DST_CLEAR,
    TEXT_QUEUE_DST_ASSIGN,
}
text_queue_action_e;

typedef struct bg_scroll_data
{
    uint old_offset_x;
    uint old_offset_y;
    bool screen_dirty;
} bg_scroll_data_s;

typedef struct vbl_defer
{
    void (*func)(void *userdata);
    void *userdata;
}
vbl_defer_s;

OBJ_ATTR gfx_oam_buffer[128];
static u16 gfx_mul_palette[16];

EWRAM_BSS gfx_bg_s gfx_bg[4];

#pragma endregion









//------------------------------------------------------------------------------
// palettes
//------------------------------------------------------------------------------
#pragma region palettes

EWRAM_BSS u16 gfx_palette[16];

#define RAINBOW_PALETTE_LENGTH (sizeof(rainbow_pal) / sizeof(*rainbow_pal))
static const int rainbow_pal[] = {
    GFX_PAL_RED, GFX_PAL_ORANGE, GFX_PAL_YELLOW, GFX_PAL_GREEN,
    GFX_PAL_BLUE, GFX_PAL_PINK
};

static int rainbow_shift = 0;
static int rainbow_shift_time_accum = 0;

static const u16 gfx_palette_normal[16] = {
    0x0000,
    0x28a4,
    0x288f,
    0x2a00,
    0x1d55,
    0x296c,
    0x6318,
    0x73bf,
    0x241f,
    0x029f,
    0x17bf,
    0x1f80,
    0x7ea5,
    0x4dd0,
    0x51df,
    0x573f
};

static const u16 gfx_palette_corrected[16] = {
    0x0000,
    0x3807,
    0x3452,
    0x3e80,
    0x0d19,
    0x2d8e,
    0x779c,
    0x6fbf,
    0x241f,
    0x029f,
    0x039f,
    0x1fe0,
    0x7e60,
    0x5e12,
    0x455f,
    0x4aff
};

static void update_rainbow_palette(void)
{
    int j = rainbow_shift;
    for (int i = 0; i < 16; ++i)
    {
        pal_obj_bank[1][i] = gfx_mul_palette[rainbow_pal[j]];
        if (++j == RAINBOW_PALETTE_LENGTH) j = 0;
    }
}

void gfx_set_palette_mode(gfx_pal_mode_e mode)
{
    switch (mode)
    {
        case GFX_PAL_MODE_NORMAL:
            memcpy16(gfx_palette, gfx_palette_normal, 16);
            break;

        case GFX_PAL_MODE_LCD_CORRECTED:
            memcpy16(gfx_palette, gfx_palette_corrected, 16);
            break;

        default: return;
    }

    gfx_reset_palette();
}

void gfx_reset_palette(void)
{
    for (int i = 0; i < 16; ++i)
        gfx_mul_palette[i] = gfx_palette[i];

    // pal bank 0: regular palette, may be darkened for room transition
    // pal bank 1: regular palette, with idx15 (peach) swapped out for black.
    //             also color index 0 *must* be black, so that 8bpp mode can
    //             use idx 16 to refer to black.
    // pal bank 2: 1-black, 2-yellow, 3-light blue, 15-white. used for text.
    // pal bank 3: same as pal bank 2, but may be darkened for room transition.
    for (int i = 0; i < 16; ++i)
        pal_bg_bank[0][i] = gfx_palette[i];

    pal_bg_bank[1][0] = gfx_palette[GFX_PAL_BLACK];
    for (int i = 1; i < 16; ++i)
        pal_bg_bank[1][i] = gfx_palette[i];
    pal_bg_bank[1][15] = gfx_palette[GFX_PAL_BLACK];

    pal_bg_bank[2][1]  = gfx_palette[0];
    pal_bg_bank[2][2]  = gfx_palette[GFX_PAL_YELLOW];
    pal_bg_bank[2][3]  = gfx_palette[GFX_PAL_BLUE];
    pal_bg_bank[2][15] = gfx_palette[GFX_PAL_WHITE];

    pal_bg_bank[3][1]  = gfx_palette[0];
    pal_bg_bank[3][2]  = gfx_palette[GFX_PAL_YELLOW];
    pal_bg_bank[3][3]  = gfx_palette[GFX_PAL_BLUE];
    pal_bg_bank[3][15] = gfx_palette[GFX_PAL_WHITE];

    // pal bank 0: regular palette, may be darkened for room transition
    // pal bank 1: dynamically changing rainbow palette
    // pal bank 2: regular palette, but never darkened
    for (int i = 0; i < 16; ++i)
        pal_obj_bank[0][i] = gfx_palette[i];

    for (int i = 0; i < 16; ++i)
        pal_obj_bank[2][i] = gfx_palette[i];

    update_rainbow_palette();

    for (int i = 2; i < 16; ++i)
        pal_bg_bank[i][0] = RGB8(255, 0, 255);

    for (int i = 0; i < 16; ++i)
        pal_obj_bank[i][0] = RGB8(255, 0, 255);
}

ARM_FUNC NO_INLINE
void gfx_set_palette_multiplied(FIXED factor)
{
    if      (factor < 0)       factor = 0;
    else if (factor > FIX_ONE) factor = FIX_ONE;

    // const FIXED scale_factor = TO_FIXED(256.0 / 31.0) + 1;

    for (int i = 1; i < 16; ++i)
    {
        int color = gfx_palette[i];
        FIXED r = (2 * FIX_ONE) * (color & 0x1F);
        FIXED g = (2 * FIX_ONE) * ((color >> 5) & 0x1F);
        FIXED b = (2 * FIX_ONE) * ((color >> 10) & 0x1F);
        r = fxmul(r, factor);
        g = fxmul(g, factor);
        b = fxmul(b, factor);

        FIXED min_dist_sq = INT32_MAX;
        int color_index = 0;

        for (int j = 0; j < 16; ++j)
        {
            int ocolor = gfx_palette[j];
            FIXED or = (2 * FIX_ONE) * (ocolor & 0x1F);
            FIXED og = (2 * FIX_ONE) * ((ocolor >> 5) & 0x1F);
            FIXED ob = (2 * FIX_ONE) * ((ocolor >> 10) & 0x1F);

            FIXED dr = or - r;
            FIXED dg = og - g;
            FIXED db = ob - b;

            FIXED dist_sq = fxmul(dr, dr) + fxmul(dg, dg) + fxmul(db, db);
            if (dist_sq < min_dist_sq)
            {
                color_index = j;
                min_dist_sq = dist_sq;
            }
        }

        gfx_mul_palette[i] = gfx_palette[color_index];
    }

    for (int i = 1; i < 16; ++i)
        pal_bg_bank[0][i] = gfx_mul_palette[i];

    pal_bg_bank[3][1]  = gfx_mul_palette[0];
    pal_bg_bank[3][2]  = gfx_mul_palette[GFX_PAL_YELLOW];
    pal_bg_bank[3][3]  = gfx_mul_palette[GFX_PAL_BLUE];
    pal_bg_bank[3][15] = gfx_mul_palette[GFX_PAL_WHITE];

    for (int i = 1; i < 16; ++i)
        pal_obj_bank[0][i] = gfx_mul_palette[i];

    update_rainbow_palette();
}

#pragma endregion









//------------------------------------------------------------------------------
// map scrolling
//------------------------------------------------------------------------------
#pragma region map scrolling

static EWRAM_BSS bg_scroll_data_s bg_scroll_data[4];

static void write_scr_block(const uint map_entry, u32 *const dest)
{
    if (map_entry == 0)
    {
        *dest        = 0;
        *(dest + 16) = 0;
        return;
    }

    int gfx_id = map_entry & 0xFF;
    int v = gfx_id % 16 * 2 + gfx_id / 16 * 64;
    v = (v - 1) + GFX_CHAR_GAME_TILESET / 2;

    u32 upper = ((v+1) << 16) | v;
    v += 32;
    u32 lower = ((v+1) << 16) | v;

    if (map_entry & 0x100) // horiz flip
    {
        upper = ((upper & 0xFFFF) << 16) | (upper >> 16);
        lower = ((lower & 0xFFFF) << 16) | (lower >> 16);
        upper = upper | SE_HFLIP | (SE_HFLIP << 16);
        lower = lower | SE_HFLIP | (SE_HFLIP << 16);
    }

    if (map_entry & 0x200) // vert flip
    {
        u32 temp = upper;
        upper = lower;
        lower = temp;
        upper |= SE_VFLIP | (SE_VFLIP << 16);
        lower |= SE_VFLIP | (SE_VFLIP << 16);
    }

    *dest = upper;
    *(dest + 16) = lower;;
}

static void update_map_scroll(uint bg_idx)
{
    static const uint gfx_bg_indices[4] = {
        GFX_BG0_INDEX, GFX_BG1_INDEX, GFX_BG2_INDEX, GFX_BG3_INDEX
    };

    gfx_bg_s *bg = gfx_bg + bg_idx;
    bg_scroll_data_s *scroll_data = bg_scroll_data + bg_idx;

    if (bg->map)
    {
        const u16 *map_data = map_graphics_data(bg->map);
        u32 *se32 = (u32 *)se_mem[gfx_bg_indices[bg_idx]];

        int prev_cam_tx = scroll_data->old_offset_x / 16;
        int cam_tx = bg->offset_x / 16;

        int prev_cam_ty = scroll_data->old_offset_y / 16;
        int cam_ty = bg->offset_y / 16;

        if (scroll_data->screen_dirty)
        {
            scroll_data->screen_dirty = false;
            int ey = cam_ty + SCRH_T16 + 1;
            int ex = cam_tx + SCRW_T16 + 1;

            for (int y = cam_ty; y < ey; ++y)
            {
                for (int x = cam_tx; x < ex; ++x)
                {
                    uint ii = y * bg->map_width + x;
                    uint oi = (y % 16) * 32 + (x % 16);
                    uint entry = (uint) map_data[ii];
                    write_scr_block(entry, se32 + oi);
                }
            }
        }
        else
        {
            // x scrolling (also handles corners)
            if (cam_tx != prev_cam_tx)
            {
                int sx, ex;
                if (cam_tx > prev_cam_tx)
                {
                    sx = prev_cam_tx + SCRW_T16 + 1;
                    ex = cam_tx + SCRW_T16;
                }
                else
                {
                    sx = cam_tx;
                    ex = prev_cam_tx;
                }

                int sy = cam_ty;
                int ey = cam_ty + SCRH_T16;

                for (int x = sx; x <= ex; ++x)
                {
                    for (int y = sy; y <= ey; ++y)
                    {
                        uint ii = y * bg->map_width + x;
                        uint oi = (y % 16) * 32 + (x % 16);
                        uint entry = (uint) map_data[ii];
                        write_scr_block(entry, se32 + oi);
                    }
                }
            }

            // y scrolling
            if (cam_ty != prev_cam_ty)
            {
                int sy, ey;
                if (cam_ty > prev_cam_ty)
                {
                    sy = prev_cam_ty + SCRH_T16 + 1;
                    ey = cam_ty + SCRH_T16;
                }
                else
                {
                    sy = cam_ty - 1;
                    ey = prev_cam_ty - 1;
                }

                int sx = cam_tx;
                int ex = cam_tx + SCRW_T16;

                for (int y = sy; y <= ey; ++y)
                {
                    for (int x = sx; x <= ex; ++x)
                    {
                        uint ii = y * bg->map_width + x;
                        uint oi = (y % 16) * 32 + (x % 16);
                        uint entry = (uint) map_data[ii];
                        write_scr_block(entry, se32 + oi);
                    }
                }
            }
        }
    }

    scroll_data->old_offset_x = bg->offset_x;
    scroll_data->old_offset_y = bg->offset_y;
}

void gfx_mark_scroll_dirty(uint bg_idx)
{
    bg_scroll_data[bg_idx].screen_dirty = true;
}

void gfx_load_map(uint bg_idx, const map_header_s *map)
{
    gfx_bg_s *bg = gfx_bg + bg_idx;
    bg_scroll_data_s *sdata = bg_scroll_data + bg_idx;

    bg->map = map;
    bg->map_width = map->width;
    bg->map_height = map->height;
    sdata->screen_dirty = true;
}

#pragma endregion









//------------------------------------------------------------------------------
// text engine
//------------------------------------------------------------------------------
#pragma region text engine

typedef struct text_cmd_header
{
    u8 action;
}
text_cmd_header_s;

typedef struct text_cmd_blit
{
    text_cmd_header_s head;

    uint x, y;
    const TILE4 *src_tile;
}
text_cmd_blit_s;

typedef struct text_cmd_blit_colored
{
    text_cmd_header_s head;

    uint x, y;
    const TILE4 *src_tile;
    u32 color_bits;
}
text_cmd_blit_colored_s;

typedef struct text_cmd_fill
{
    text_cmd_header_s head;

    uint oc, or, cols, rows;
    u32 data[8];
}
text_cmd_fill_s;

typedef struct text_cmd_dst_clear
{
    text_cmd_header_s head;

    uint row, row_count;
} text_cmd_dst_clear_s;

typedef struct text_cmd_dst_assign
{
    text_cmd_header_s head;

    uint row, row_count, src_row, pal;
} text_cmd_dst_assign_s;

static u8 *text_queue_write;
static EWRAM_BSS u8 text_queue[TEXT_QUEUE_MAX_SIZE];
#define TEXT_QUEUE_END (text_queue + TEXT_QUEUE_MAX_SIZE)

#define TEXT_ENQUEUE_START(p_action, sname)                                    \
    u8 *e = text_queue_write + sizeof(text_cmd_##sname##_s);                   \
    if (e > TEXT_QUEUE_END)                                                    \
    {                                                                          \
        LOG_WRN("text command queue is full!");                                \
        return false;                                                          \
    }                                                                          \
                                                                               \
    text_cmd_##sname##_s *alloc = (void *)text_queue_write;                    \
    text_queue_write = (void *)align((uint)e, alignof(text_cmd_header_s));     \
    *alloc = (text_cmd_##sname##_s)                                            \
    {                                                                          \
        .head.action = p_action,                                               \

#define TEXT_ENQUEUE_END }; return true;

bool gfx_text_bmap_fill(uint oc, uint or_, uint cols, uint rows, u32 data[8])
{
    TEXT_ENQUEUE_START(TEXT_QUEUE_FILL, fill)
        .oc = oc,
        .or = or_,
        .cols = cols,
        .rows = rows,
        .data[0] = data[0],
        .data[1] = data[1],
        .data[2] = data[2],
        .data[3] = data[3],
        .data[4] = data[4],
        .data[5] = data[5],
        .data[6] = data[6],
        .data[7] = data[7]
    TEXT_ENQUEUE_END
}

static bool blit_tile(uint x, uint y, const TILE *src_tile)
{
    TEXT_ENQUEUE_START(TEXT_QUEUE_BLIT, blit)
        .x = x,
        .y = y,
        .src_tile = src_tile
    TEXT_ENQUEUE_END
}

static bool blit_tile_colored(uint x, uint y, const TILE *src_tile,
                              u32 color_bits)
{
    TEXT_ENQUEUE_START(TEXT_QUEUE_BLIT_COLORED, blit_colored)
        .x = x,
        .y = y,
        .src_tile = src_tile,
        .color_bits = color_bits
    TEXT_ENQUEUE_END
}

bool gfx_text_bmap_dst_clear(uint row, uint row_count)
{
    TEXT_ENQUEUE_START(TEXT_QUEUE_DST_CLEAR, dst_clear)
        .row = row,
        .row_count = row_count
    TEXT_ENQUEUE_END
}

bool gfx_text_bmap_dst_assign(uint row, uint row_count, uint src_row, uint pal)
{
    TEXT_ENQUEUE_START(TEXT_QUEUE_DST_ASSIGN, dst_assign)
        .row = row,
        .row_count = row_count,
        .src_row = src_row,
        .pal = pal,
    TEXT_ENQUEUE_END
}

bool gfx_text_bmap_print(uint x, uint y, const char *text,
                         text_color_e text_color)
{
    const TILE *const text_data = (const TILE *)font_gfxTiles;

    bool is_white = text_color == TEXT_COLOR_WHITE;
    u32 color_bits = 0;
    if (text_color == TEXT_COLOR_BLUE)
        color_bits = 0x33333333;
    else if (text_color == TEXT_COLOR_YELLOW)
        color_bits = 0x22222222;
    else if (text_color == TEXT_COLOR_BLACK)
        color_bits = 0x11111111;

    #define ERRCHK(res) \
        if (!(res)) goto err;

    for (; *text != '\0'; ++text)
    {
        char ch = *text;
        uint id;
        switch (ch)
        {
        case ' ':
            goto next_char;
            break;

        case '.':
            id = TEXT_CHAR_ID(36);
            break;

        case ',':
            id = TEXT_CHAR_ID(37);
            break;

        case '!':
            id = TEXT_CHAR_ID(38);
            break;

        case '?':
            id = TEXT_CHAR_ID(39);
            break;

        case '-':
            id = TEXT_CHAR_ID(40);
            break;

        case '_':
            id = TEXT_CHAR_ID(41);
            break;

        case ':':
            id = TEXT_CHAR_ID(42);
            break;
        
        case '*': // not actually the asterisk character but whatever. sure.
            id = TEXT_CHAR_ID(43);
            break;
        
        case '\'':
            id = TEXT_CHAR_ID(45);
            break;
        
        case '/':
            id = TEXT_CHAR_ID(46);
            break;
        
        case '"':
            id = TEXT_CHAR_ID(47);
            break;
        
        case '%':
            id = TEXT_CHAR_ID(48);
            break;
        
        case '\x7F':
            id = TEXT_CHAR_ID(44);
            break;

        // assume [a-zA-Z0-9]
        default:
            if (ch >= '0' && ch <= '9')
                id = TEXT_CHAR_ID(ch - '0' + 26);
            else
                // assume it's a letter
                id = TEXT_CHAR_ID(toupper(ch) - 'A');
        }

        const TILE *src_tile = text_data + id;

        if (is_white)
        {
            ERRCHK(blit_tile(x, y, src_tile));
            ERRCHK(blit_tile(x + 8, y, ++src_tile));
            ERRCHK(blit_tile(x, y + 8, ++src_tile));
            ERRCHK(blit_tile(x + 8, y + 8, ++src_tile));
        }
        else
        {
            ERRCHK(blit_tile_colored(x, y, src_tile, color_bits));
            ERRCHK(blit_tile_colored(x + 8, y, ++src_tile, color_bits));
            ERRCHK(blit_tile_colored(x, y + 8, ++src_tile, color_bits));
            ERRCHK(blit_tile_colored(x + 8, y + 8, ++src_tile, color_bits));
        }

        next_char:;
        x += 12;
    }

    return true;
    err:
        return false;
}

ARM_FUNC NO_INLINE
static void _gfx_blit_tile(uint x, uint y, const TILE4 *src_tile)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &GFX_TEXT_BMP_VRAM[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
    const u32 *src_row = src_tile->data;

    // bool *dirty = text_bmp_dirty_rows + (y >> 3);
    // *dirty = true;

    if (shf)
    {
        for (int r = 0; r < 8; ++r)
        {
            u32 src = *src_row;
            *row |= src << shf;
            *(row + 8) |= src >> (32 - shf);

            ++row;
            ++src_row;
            if (++ry == 8)
            {
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
                // *(++dirty) = true;
            }
        }
    }
    else
    {
        for (int r = 0; r < 8; ++r)
        {
            u32 src = *src_row;
            *row |= src;

            ++row;
            ++src_row;
            if (++ry == 8)
            {
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
                // *(++dirty) = true;
            }
        }
    }
}

ARM_FUNC NO_INLINE
static void _gfx_blit_tile_colored(uint x, uint y, const TILE4 *src_tile,
                                   u32 color_bits)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &GFX_TEXT_BMP_VRAM[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
    const u32 *src_row = src_tile->data;

    // bool *dirty = text_bmp_dirty_rows + (y >> 3);
    // *dirty = true;
    
    if (shf)
    {
        for (int r = 0; r < 8; ++r)
        {
            u32 src = *src_row;
            u32 blit = src << shf;
            *row = (*row & ~blit) | (blit & color_bits);

            row += 8;
            blit = src >> (32 - shf);
            *row = (*row & ~blit) | (blit & color_bits);

            row -= 7;
            ++src_row;
            if (++ry == 8)
            {
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
                // *(++dirty) = true;
            }
        }
    }
    else
    {
        for (int r = 0; r < 8; ++r)
        {
            u32 src = *src_row;
            *row = (*row & ~src) | (src & color_bits);

            ++row;
            ++src_row;
            if (++ry == 8)
            {
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
                // *(++dirty) = true;
            }
        }
    }
    
    // for (int r = 0; r < 8; ++r)
    // {
    //     u32 src_row = src_tile->data[r];
    //     u32 *row = get_pixel_row(dx, dy);
    //     uint shf = (dx & 7) << 2;
    //     u32 blit = src_row << shf;
    //     *row = (*row & ~blit) | (blit & color_bits);

    //     if (shf) // shf > 0
    //     {
    //         row = get_pixel_row(dx + 8, dy);
    //         shf = 32 - shf;
    //         blit = src_row >> shf;
    //         *row = (*row & ~blit) | (blit & color_bits);
    //     }

    //     ++dy;
    // }
}

// written in asm. For funsies.
ARM_FUNC
void _gfx_text_bmap_fill(uint oc, uint or_, uint cols, uint rows, u32 data[8]);

ARM_FUNC NO_INLINE
static void _gfx_text_bmap_dst_clear(uint row, uint row_count)
{
    uint i = row * 32;
    for (uint y = row; y < row + row_count; ++y)
    {
        for (uint x = 0; x < 30; ++x)
            se_mem[GFX_BG0_INDEX][i++] = 0;
        i += 2;
    }
}

ARM_FUNC NO_INLINE
static void _gfx_text_bmap_dst_assign(uint row, uint row_count, uint src_row,   
                                      uint pal)
{
    uint i = src_row * GFX_TEXT_BMP_COLS + 1;
    uint j = row * 32;
    for (uint y = row; y < row + row_count; ++y)
    {
        for (uint x = 0; x < 30; ++x)
            se_mem[GFX_BG0_INDEX][j++] = SE_PALBANK(pal) | SE_ID(i++);
        j += 2;
    }
}

// IWRAM_CODE ARM_FUNC NO_INLINE
static void flush_text_cmds(void)
{
    for (u8 *ptr = text_queue; ptr < text_queue_write;)
    {
        text_cmd_header_s *header = (text_cmd_header_s *)ptr;
        switch (header->action)
        {
        case TEXT_QUEUE_BLIT:
        {
            text_cmd_blit_s *data = (void *)header;
            _gfx_blit_tile(data->x, data->y, data->src_tile);
            ptr += sizeof(*data);
            break;
        }
        
        case TEXT_QUEUE_BLIT_COLORED:
        {
            text_cmd_blit_colored_s *data = (void *)header;
            _gfx_blit_tile_colored(data->x, data->y,
                                   data->src_tile,
                                   data->color_bits);
            ptr += sizeof(*data);
            break;
        }
        
        case TEXT_QUEUE_FILL:
        {
            text_cmd_fill_s *data = (void *)header;
            _gfx_text_bmap_fill(data->oc, data->or, data->cols, data->rows,
                                data->data);
            ptr += sizeof(*data);
            break;
        }

        case TEXT_QUEUE_DST_ASSIGN:
        {
            text_cmd_dst_assign_s *data = (void *)header;
            _gfx_text_bmap_dst_assign(data->row, data->row_count, data->src_row,
                                      data->pal);
            ptr += sizeof(*data);
            break;
        }
        
        case TEXT_QUEUE_DST_CLEAR:
        {
            text_cmd_dst_clear_s *data = (void *)header;
            _gfx_text_bmap_dst_clear(data->row, data->row_count);
            ptr += sizeof(*data);
            break;
        }
        
        default:
            LOG_ERR("unknown text command %u", header->action);
            goto exit_text_queue_flush;
        }

        ptr = (u8 *)align((uint)ptr, alignof(text_cmd_header_s));
    }

exit_text_queue_flush:
    text_queue_write = text_queue;
}

#pragma endregion









//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
#pragma region main

static uint defer_queue_size = 0;
static EWRAM_BSS vbl_defer_s defer_queue[DEFER_QUEUE_MAX_SIZE];

void gfx_init(void)
{
    text_queue_write = text_queue;
    memcpy16(gfx_palette, gfx_palette_normal, 16);
    oam_init(gfx_oam_buffer, 128);
    gfx_reset_palette();

    for (uint i = 0; i < 4; ++i)
    {
        gfx_bg[i] = (gfx_bg_s)
        {
            .bpp = GFX_BG_4BPP,
            .enabled = false,
        };

        bg_scroll_data[i] = (bg_scroll_data_s){0};
    };

    gfx_bg[0].enabled = true;
    gfx_bg[0].char_block = GFX_TEXT_BMP_BLOCK;
    gfx_bg[0].priority = 0;
    gfx_bg[1].priority = 1;
    gfx_bg[2].priority = 2;
    gfx_bg[3].priority = 2;

    // REG_DISPCNT = DCNT_OBJ_1D;

    // REG_BG0CNT = BG_CBB(GFX_TEXT_BMP_BLOCK) | BG_SBB(GFX_BG0_INDEX) | BG_4BPP |
    //              BG_REG_32x32 | BG_PRIO(0);
    // REG_BG0HOFS = 0;
    // REG_BG0VOFS = 0;

    // REG_BG1CNT = BG_CBB(0) | BG_SBB(GFX_BG1_INDEX) | BG_8BPP | BG_REG_32x32 |
    //              BG_PRIO(1);
    // REG_BG1HOFS = 0;
    // REG_BG1VOFS = 0;

    // REG_BG2CNT = BG_SBB(GFX_BG2_INDEX) | BG_4BPP | BG_PRIO(2);
    // REG_BG3CNT = BG_SBB(GFX_BG2_INDEX) | BG_4BPP | BG_PRIO(2);
}

bool gfx_defer_vblank(void (*func)(void *userdata), void *userdata)
{
    if (defer_queue_size == DEFER_QUEUE_MAX_SIZE)
        return false;

    defer_queue[defer_queue_size++] = (vbl_defer_s)
    {
        .func = func,
        .userdata = userdata
    };

    return true;
}

void gfx_new_frame(void)
{
    if (++rainbow_shift_time_accum == 4)
    {
        rainbow_shift_time_accum = 0;
        if (++rainbow_shift == RAINBOW_PALETTE_LENGTH) rainbow_shift = 0;
        update_rainbow_palette();
    }

    for (uint i = 0; i < 4; ++i)
        update_map_scroll(i);

    oam_copy(oam_mem, gfx_oam_buffer, 128);

    for (uint i = 0; i < defer_queue_size; ++i)
        defer_queue[i].func(defer_queue[i].userdata);
    defer_queue_size = 0;

    flush_text_cmds();

    REG_BG0HOFS = gfx_bg[0].offset_x;
    REG_BG0VOFS = gfx_bg[0].offset_y;
    REG_BG1HOFS = gfx_bg[1].offset_x;
    REG_BG1VOFS = gfx_bg[1].offset_y;
    REG_BG2HOFS = gfx_bg[2].offset_x;
    REG_BG2VOFS = gfx_bg[2].offset_y;
    REG_BG3HOFS = gfx_bg[3].offset_x;
    REG_BG3VOFS = gfx_bg[3].offset_y;
    
    u32 reg_dispcnt = DCNT_OBJ | DCNT_OBJ_1D;
    u16 bg_cnt[4] = { 0, 0, 0, 0 };
    
    bg_cnt[0] = BG_SBB(GFX_BG0_INDEX) | BG_REG_32x32;
    bg_cnt[1] = BG_SBB(GFX_BG1_INDEX) | BG_REG_32x32;
    bg_cnt[2] = BG_SBB(GFX_BG2_INDEX) | BG_REG_32x32;
    bg_cnt[3] = BG_SBB(GFX_BG2_INDEX) | BG_REG_32x32;

    for (uint i = 0; i < 4; ++i)
    {
        gfx_bg_s *config = gfx_bg + i;
        if (config->enabled)
            reg_dispcnt |= DCNT_BG0 << i;

        u16 *const cnt = bg_cnt + i;

        *cnt |= BG_PRIO(config->priority) | BG_CBB(config->char_block);
        if (config->bpp == GFX_BG_8BPP)
            *cnt |= BG_8BPP;
        else
            *cnt |= BG_4BPP;
    }

    REG_DISPCNT = reg_dispcnt;
    REG_BG0CNT = bg_cnt[0];
    REG_BG1CNT = bg_cnt[1];
    REG_BG2CNT = bg_cnt[2];
    REG_BG3CNT = bg_cnt[3];

#ifdef DEVDEBUG
    if (REG_VCOUNT < 160)
        LOG_WRN("gfx_new_frame took too long! %i", REG_VCOUNT);
#endif
}

void gfx_draw_sprite(gfx_draw_sprite_state_s *state, uint spr_idx,
                     uint frame_idx, int draw_x, int draw_y)
{
    const gfx_sprdb_s *sprdb = state->sprdb;

    const gfx_sprite_s *spr = &sprdb->gfx_sprites[spr_idx];
    const gfx_frame_s *frame = sprdb->frame_pool + spr->frame_pool_idx + frame_idx;
    const gfx_obj_s *objs = sprdb->obj_pool + frame->obj_pool_index;
    
    int frame_obj_count = frame->obj_count;

    const bool hflip = state->a1 & ATTR1_HFLIP;
    const bool vflip = state->a1 & ATTR1_VFLIP;

    // draw object assembly
    for (int j = 0; j < frame_obj_count; ++j)
    {
        if (state->dst_obj_count == 0) break;

        const gfx_obj_s *obj_src = &objs[j];

        int ox, oy;

        if (hflip)
            ox = obj_src->flipped_ox;
        else
            ox = obj_src->ox;

        if (vflip)
            oy = obj_src->flipped_oy;
        else
            oy = obj_src->oy;

        u16 final_a0 = obj_src->a0 | state->a0;
        u16 final_a1 = obj_src->a1 | state->a1;
        u16 final_a2 = obj_src->a2 | state->a2;

        obj_set_attr(state->dst_obj, final_a0, final_a1, final_a2);
        obj_set_pos(state->dst_obj, draw_x + ox, draw_y + oy);
        ++state->dst_obj;
        --state->dst_obj_count;
    }
}

#pragma endregion