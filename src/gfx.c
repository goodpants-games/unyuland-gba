#include <tonc.h>
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
#define DMA_QUEUE_MAX_SIZE 32
#define DMA_CPYPOOL_SIZE (4096 / 4)
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
        int color = gfx_palette_normal[i];
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
            int ocolor = gfx_palette_normal[j];
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

typedef struct bg_scroll_data
{
    uint old_offset_x;
    uint old_offset_y;
    bool screen_dirty;
} bg_scroll_data_s;

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

EWRAM_BSS TILE gfx_text_bmp_buf[GFX_TEXT_BMP_SIZE];
EWRAM_BSS bool gfx_text_bmp_dirty_rows[GFX_TEXT_BMP_ROWS];

ARM_FUNC void _gfx_text_blit_tile(uint x, uint y, const TILE4 *src_tile);

/*
ARM_FUNC NO_INLINE
static void _gfx_text_blit_tileC(uint x, uint y, const TILE4 *src_tile)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &gfx_text_bmp_buf[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
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
*/

ARM_FUNC NO_INLINE
static void blit_tile_colored(uint x, uint y, const TILE4 *src_tile,
                              u32 color_bits)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &gfx_text_bmp_buf[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
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

ARM_FUNC NO_INLINE
void gfx_text_bmap_dst_clear(uint row, uint row_count)
{
    gfx_queue_memset(&se_mem[GFX_BG0_INDEX][row * 32], 0,
                     sizeof(SCR_ENTRY) * row_count * 32);
}

ARM_FUNC NO_INLINE
void gfx_text_bmap_dst_assign(uint row, uint row_count, uint src_row,   
                                      uint pal)
{
    uint i = src_row * GFX_TEXT_BMP_COLS + 1;
    
    const size_t sz = sizeof(SCR_ENTRY) * 32 * row_count;
    SCR_ENTRY *alloc = gfx_alloc_cpybuf(sz);
    if (!alloc) return;

    uint j = 0;
    for (uint y = 0; y < row_count; ++y)
    {
        for (uint x = 0; x < 30; ++x)
            alloc[j++] = SE_PALBANK(pal) | SE_ID(i++);
        j += 2;
    }

    gfx_queue_memcpy(&se_mem[GFX_BG0_INDEX][row * 32], alloc, sz);
}

/*
#include <stdio.h>
#include <ctype.h>

#define TEXT_CHAR_ID(i) ((i) * 4)

static int get_id(char ch)
{
    unsigned int id;
    
    switch (ch)
    {
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
        else if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
            // assume it's a letter
            id = TEXT_CHAR_ID(toupper(ch) - 'A');
        else
            id = 0;
        
        break;
    }
    
    return id;
}

int main()
{
    for (int ch = 0; ch < 256; ch++)
    {
        if (ch % 8 == 0) printf("\n");
        printf("0x%02X,", get_id((char) ch));
    }
    
    printf("\n");

    return 0;
}
*/

static const u8 char_map[256] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x98,0xBC,0x00,0x00,0xC0,0x00,0xB4,
    0x00,0x00,0xAC,0x00,0x94,0xA0,0x90,0xB8,
    0x68,0x6C,0x70,0x74,0x78,0x7C,0x80,0x84,
    0x88,0x8C,0xA8,0x00,0x00,0x00,0x00,0x9C,
    0x00,0x00,0x04,0x08,0x0C,0x10,0x14,0x18,
    0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,
    0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,
    0x5C,0x60,0x64,0x00,0x00,0x00,0x00,0xA4,
    0x00,0x00,0x04,0x08,0x0C,0x10,0x14,0x18,
    0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,
    0x3C,0x40,0x44,0x48,0x4C,0x50,0x54,0x58,
    0x5C,0x60,0x64,0x00,0x00,0x00,0x00,0xB0,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

ARM_FUNC NO_INLINE
void gfx_text_bmap_print(uint x, uint y, const char *text,
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
    
    gfx_text_bmp_dirty_rows[y / 8] = true;
    gfx_text_bmp_dirty_rows[y / 8 + 1] = true;
    gfx_text_bmp_dirty_rows[(y + 15) / 8] = true;

    for (; *text != '\0'; ++text)
    {
        char ch = *text;
        if (ch == ' ') goto next_char;

        uint id = char_map[(u8) ch];

        const TILE *src_tile = text_data + id;

        if (is_white)
        {
            _gfx_text_blit_tile(x, y, src_tile);
            _gfx_text_blit_tile(x + 8, y, ++src_tile);
            _gfx_text_blit_tile(x, y + 8, ++src_tile);
            _gfx_text_blit_tile(x + 8, y + 8, ++src_tile);
        }
        else
        {
            blit_tile_colored(x, y, src_tile, color_bits);
            blit_tile_colored(x + 8, y, ++src_tile, color_bits);
            blit_tile_colored(x, y + 8, ++src_tile, color_bits);
            blit_tile_colored(x + 8, y + 8, ++src_tile, color_bits);
        }

        next_char:;
        x += 12;
    }
}

#pragma endregion







//------------------------------------------------------------------------------
// dma
//------------------------------------------------------------------------------
#pragma region dma

typedef enum dma_req_t
{
    GFX_DMA_MEMCPY32,
    GFX_DMA_MEMSET32
}
dma_req_t_e;

typedef struct gfx_dma_req
{
    u8 type;
    void *dst;
    union
    {
        const void *src;
        uint value;
    };
    size_t count;
}
gfx_dma_req_s;

static uint dma_queue_size = 0;
static EWRAM_BSS gfx_dma_req_s dma_queue[DMA_QUEUE_MAX_SIZE];
static u32 *dma_cpypool_write;
static EWRAM_BSS u32 dma_cpypool[DMA_CPYPOOL_SIZE];

#define CHECK_DMA_QUEUE()  \
    if (dma_queue_size == DMA_QUEUE_MAX_SIZE)  \
    {  \
        LOG_ERR("DMA queue is full!");  \
        return false;  \
    }

bool gfx_queue_memcpy(void *dst, const void *src, size_t size)
{
    CHECK_DMA_QUEUE();

    dma_queue[dma_queue_size++] = (gfx_dma_req_s)
    {
        .type = GFX_DMA_MEMCPY32,
        .dst = dst,
        .src = src,
        .count = size
    };

    return true;
}

bool gfx_queue_memset(void *dst, u8 value, size_t size)
{
    CHECK_DMA_QUEUE();

    dma_queue[dma_queue_size++] = (gfx_dma_req_s)
    {
        .type = GFX_DMA_MEMSET32,
        .dst = dst,
        .value = (uint) value,
        .count = size
    };

    return true;
}

static void flush_dma_queue(void)
{
    for (uint i = 0; i < dma_queue_size; ++i)
    {
        gfx_dma_req_s *req = dma_queue + i;

        if (req->type == GFX_DMA_MEMCPY32)
        {
            dma3_cpy(req->dst, req->src, req->count);
            if (req->count & 3) // if not word-aligned, copy remainder
            {
                size_t s = req->count & ~3;
                u8 *const dst = req->dst;
                const u8 *const src = req->src;

                dst[s] = src[s]; ++s;
                dst[s] = src[s]; ++s;
                dst[s] = src[s];
            }
        }
        else if (req->type == GFX_DMA_MEMSET32)
        {
            dma3_fill(req->dst, req->value, req->count);
            if (req->count & 3) // if not word-aligned, fill remainder
            {
                size_t s = req->count & ~3;
                u8 *const dst = req->dst;
                
                dst[s++] = req->value;
                dst[s++] = req->value;
                dst[s]   = req->value;
            }
        }
        else
        {
            LOG_ERR("unknown gfx copy type %u", req->type);
        }
    }
    dma_queue_size = 0;
    dma_cpypool_write = dma_cpypool;
}

void* gfx_alloc_cpybuf(size_t size)
{
    size_t wsize = CEIL_DIV(size, 4);

    u32 *e = dma_cpypool_write + wsize;
    if (e > dma_cpypool + DMA_CPYPOOL_SIZE)
    {
        LOG_ERR("gfx dma copy pool full!");
        return NULL;
    }

    u32 *alloc = dma_cpypool_write;
    dma_cpypool_write += wsize;

    return (void *)alloc;
}

#pragma endregion dma









//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
#pragma region main

void gfx_init(void)
{
    dma_cpypool_write = dma_cpypool;
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

    flush_dma_queue();

    uint text_dma_copies = 0;

    for (uint r0 = 0; r0 < GFX_TEXT_BMP_ROWS;)
    {
        if (!gfx_text_bmp_dirty_rows[r0])
        {
            ++r0;
            continue;
        }
        
        uint r1 = r0 + 1;
        while (r1 < GFX_TEXT_BMP_ROWS && gfx_text_bmp_dirty_rows[r1])
            ++r1;

        const uint ofs = r0 * GFX_TEXT_BMP_COLS;
        const uint sz = ((r1 - r0) * GFX_TEXT_BMP_COLS) * sizeof(TILE);
        dma3_cpy(GFX_TEXT_BMP_VRAM + ofs, gfx_text_bmp_buf + ofs, sz);
        ++text_dma_copies;
        r0 = r1;
    }

    if (text_dma_copies > 0)
        LOG_DBG("text dma copies: %u", text_dma_copies);

    memset(gfx_text_bmp_dirty_rows, 0, sizeof(gfx_text_bmp_dirty_rows));

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