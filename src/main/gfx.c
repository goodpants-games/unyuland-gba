#include <string.h>
#include <stdalign.h>
#include <tonc.h>
#include <platutil.h>
#include <data/graphics/font_gfx.h>
#include <data/color_qlut_bin.h>
#include "gfx.h"
#include "math_util.h"










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

gfx_display_control_s gfx_ctl;
OBJ_ATTR gfx_oam_buffer[128];
static u16 gfx_mul_palette[16];

EWRAM_BSS static s16 last_palette_mul;

#pragma endregion









//------------------------------------------------------------------------------
// palettes
//------------------------------------------------------------------------------
#pragma region palettes

EWRAM_BSS u16 gfx_palette[16];

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
    0x4014,
    0x0340,
    0x001a,
    0x2dce,
    0x6778,
    0x73fe,
    0x301f,
    0x02df,
    0x03ff,
    0x03e0,
    0x7e20,
    0x6412,
    0x641f,
    0x477f
};

static void reset_palette(void)
{
    for (int i = 0; i < 16; ++i)
        gfx_mul_palette[i] = gfx_palette[i];

    // pal bank 0: regular palette, may be darkened for room transition
    // pal bank 1: regular palette, with idx15 (peach) swapped out for black.
    //             also color index 0 *must* be black, so that 8bpp mode can
    //             use idx 16 to refer to black. may be darkened for room
    //             transition.
    // pal bank 2: regular palette, not darkened
    // pal bank 3: 1-black, 2-yellow, 3-light blue, 15-white. used for text.
    // pal bank 4: same as pal bank 2, but may be darkened for room transition.
    // pal bank 5: bg user pal 0
    // pal bank 6: bg user pal 1
    for (int i = 0; i < 16; ++i) {
        pal_bg_bank[GFX_BGPAL_MUL][i] = gfx_palette[i];
        pal_bg_bank[GFX_BGPAL_NORMAL][i] = gfx_palette[i];
    }

    pal_bg_bank[GFX_BGPAL_BLACK_MUL][0] = gfx_palette[GFX_PAL_BLACK];
    for (int i = 1; i < 16; ++i)
        pal_bg_bank[GFX_BGPAL_BLACK_MUL][i] = gfx_palette[i];
    pal_bg_bank[GFX_BGPAL_BLACK_MUL][15] = gfx_palette[GFX_PAL_BLACK];

    pal_bg_bank[GFX_TEXTPAL_NORMAL][1]  = gfx_palette[0];
    pal_bg_bank[GFX_TEXTPAL_NORMAL][2]  = gfx_palette[GFX_PAL_YELLOW];
    pal_bg_bank[GFX_TEXTPAL_NORMAL][3]  = gfx_palette[GFX_PAL_BLUE];
    pal_bg_bank[GFX_TEXTPAL_NORMAL][15] = gfx_palette[GFX_PAL_WHITE];

    pal_bg_bank[GFX_TEXTPAL_MUL][1]  = gfx_palette[0];
    pal_bg_bank[GFX_TEXTPAL_MUL][2]  = gfx_palette[GFX_PAL_YELLOW];
    pal_bg_bank[GFX_TEXTPAL_MUL][3]  = gfx_palette[GFX_PAL_BLUE];
    pal_bg_bank[GFX_TEXTPAL_MUL][15] = gfx_palette[GFX_PAL_WHITE];

    for (uint i = 0; i < 16; ++i)
    {
        for (uint j = 0; j < GFX_BGPAL_USER_COUNT; ++j)
        {
            pal_bg_bank[GFX_BGPAL_USER0 + j][i] =
                gfx_palette[gfx_ctl.bg_userpal[j][i]];
        }
    }

    // pal bank 0: regular palette
    // pal bank 1: regular palette, dynamically multiplied
    // pal bank 2: obj user pal 0
    // pal bank 3: obj user pal 1 (multiplied)
    for (int i = 0; i < 16; ++i)
    {
        pal_obj_bank[GFX_OBJPAL_NORMAL][i] = gfx_palette[i];
        pal_obj_bank[GFX_OBJPAL_MUL][i] = gfx_mul_palette[i];

        for (uint j = 0; j < GFX_OBJPAL_USER_COUNT; ++j)
        {
            pal_obj_bank[GFX_OBJPAL_USER0 + j][i] =
                gfx_palette[gfx_ctl.obj_userpal[j][i]];
        }
    }

    // make all transparent colors display as purple in emulator debug views...
    for (int i = 2; i < 16; ++i)
        pal_bg_bank[i][0] = RGB8(255, 0, 255);

    for (int i = 0; i < 16; ++i)
        pal_obj_bank[i][0] = RGB8(255, 0, 255);

// #ifdef DEVDEBUG
//     pal_bg_bank[0][0] = RGB8(255, 0, 255);
// #endif
}

ARM_FUNC NO_INLINE
static void apply_palette_multiply(FIXED factor)
{
    if      (factor < 0)       factor = 0;
    else if (factor > FIX_ONE) factor = FIX_ONE;

    // const FIXED scale_factor = TO_FIXED(256.0 / 31.0) + 1;

    for (int i = 1; i < 16; ++i)
    {
        uint color = gfx_palette_normal[i];
        
        FIXED rf = int2fx(color & 0x1F);
        FIXED gf = int2fx((color >> 5) & 0x1F);
        FIXED bf = int2fx((color >> 10) & 0x1F);
        rf = fxmul(rf, factor);
        gf = fxmul(gf, factor);
        bf = fxmul(bf, factor);
        uint r = fx2int(rf);
        uint g = fx2int(gf);
        uint b = fx2int(bf);

        uint lut_idx = r * (32 * 32) + g * 32 + b;
        uint color_idx = (color_qlut_bin[lut_idx>>1] >> ((lut_idx&1) * 4)) & 0xF;
        gfx_mul_palette[i] = gfx_palette[color_idx];
    }

    for (int i = 1; i < 16; ++i)
    {
        pal_bg_bank[GFX_BGPAL_MUL][i] = gfx_mul_palette[i];
        pal_bg_bank[GFX_BGPAL_BLACK_MUL][i] = gfx_mul_palette[i];
    }

    pal_bg_bank[GFX_BGPAL_BLACK_MUL][GFX_PAL_PEACH]
        = gfx_mul_palette[GFX_PAL_BLACK];

    pal_bg_bank[GFX_TEXTPAL_MUL][1]  = gfx_mul_palette[0];
    pal_bg_bank[GFX_TEXTPAL_MUL][2]  = gfx_mul_palette[GFX_PAL_YELLOW];
    pal_bg_bank[GFX_TEXTPAL_MUL][3]  = gfx_mul_palette[GFX_PAL_BLUE];
    pal_bg_bank[GFX_TEXTPAL_MUL][15] = gfx_mul_palette[GFX_PAL_WHITE];

    for (int i = 1; i < 16; ++i)
        pal_obj_bank[GFX_OBJPAL_MUL][i] = gfx_mul_palette[i];

    /*
        Don't need to apply to user palettes, as they are always updated on
        every gfx_commit.
    
    // apply to bg user palettes
    for (int p = 0; p < GFX_BGPAL_USER_COUNT; ++p)
    {
        u16 *pal = (gfx_ctl.bg_userpal_mul & (1 << p)) ?
                   gfx_mul_palette : gfx_palette;
        
        for (int i = 1; i < 16; ++i)
            pal_obj_bank[GFX_OBJPAL_USER0+p][i] = pal[gfx_ctl.obj_userpal[p][i]];
    }

    // apply to obj user palettes
    for (int p = 0; p < GFX_OBJPAL_USER_COUNT; ++p)
    {
        u16 *pal = (gfx_ctl.obj_userpal_mul & (1 << p)) ?
                   gfx_mul_palette : gfx_palette;
        
        for (int i = 1; i < 16; ++i)
            pal_obj_bank[GFX_OBJPAL_USER0+p][i] = pal[gfx_ctl.obj_userpal[p][i]];
    }
    */
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

    reset_palette();
    apply_palette_multiply(gfx_ctl.palette_mul);
}

#pragma endregion









//------------------------------------------------------------------------------
// map scrolling
//------------------------------------------------------------------------------
#pragma region map scrolling

#define MAX_SCRBLOCK_WRITER_COUNT 16

typedef struct bg_scroll_data
{
    int old_offset_x;
    int old_offset_y;
    bool screen_dirty;
} bg_scroll_data_s;

static EWRAM_BSS bg_scroll_data_s bg_scroll_data[4];

static inline int calc_srcpos(int pos, int size, gfx_map_border_e border_mode)
{
    switch (border_mode)
    {
    case GFX_MAP_BORDER_CLAMP:
        if (pos < 0) pos = 0;
        else if (pos >= size) pos = size - 1;
        break;

    case GFX_MAP_BORDER_WRAP:
        while (pos < 0) pos += size;
        while (pos >= size) pos -= size;
        break;
    }

    return pos;
}

static void update_map_scroll_t(uint bg_idx, uint size_shift, uint dst_shift,
                                gfx_map_write_f write_scr_block)
{
    static const uint gfx_bg_indices[4] = {
        GFX_BG0_INDEX, GFX_BG1_INDEX, GFX_BG2_INDEX, GFX_BG3_INDEX
    };

    gfx_bg_s *bg = gfx_ctl.bg + bg_idx;
    bg_scroll_data_s *scroll_data = bg_scroll_data + bg_idx;

    const u16 *map_data = bg->map->data;
    SCR_ENTRY *se16 = se_mem[gfx_bg_indices[bg_idx]];

    int prev_cam_tx = scroll_data->old_offset_x >> size_shift;
    int cam_tx = bg->offset_x >> size_shift;

    int prev_cam_ty = scroll_data->old_offset_y >> size_shift;
    int cam_ty = bg->offset_y >> size_shift;

    uint size_mod_mask = (256 >> size_shift) - 1;

    const uint width_div = SCREEN_WIDTH >> size_shift;
    const uint height_div = SCREEN_HEIGHT >> size_shift;

    const uint map_width = bg->map_width;
    const uint map_height = bg->map_height;
    const gfx_map_border_e border_x = bg->map->border_x;
    const gfx_map_border_e border_y = bg->map->border_y;

    #define CALC_SRCPOS_X(x) calc_srcpos(x, map_width, border_x)
    #define CALC_SRCPOS_Y(y) calc_srcpos(y, map_height, border_y)

    if (scroll_data->screen_dirty)
    {
        scroll_data->screen_dirty = false;
        int ey = cam_ty + height_div + 1;
        int ex = cam_tx + width_div + 1;

        int srcx, srcy;
        for (int y = cam_ty; y < ey; ++y)
        {
            srcy = CALC_SRCPOS_Y(y);
            for (int x = cam_tx; x < ex; ++x)
            {
                srcx = CALC_SRCPOS_X(x);

                uint ii = srcy * map_width + srcx;
                uint oi = ((y & size_mod_mask) << 5) + (x & size_mod_mask);
                uint entry = (uint) map_data[ii];
                write_scr_block(entry, se16 + (oi << dst_shift));
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
                sx = prev_cam_tx + width_div + 1;
                ex = cam_tx + width_div;
            }
            else
            {
                sx = cam_tx;
                ex = prev_cam_tx;
            }

            int sy = cam_ty;
            int ey = cam_ty + height_div;

            int srcx, srcy;
            for (int x = sx; x <= ex; ++x)
            {
                srcx = CALC_SRCPOS_X(x);
                for (int y = sy; y <= ey; ++y)
                {
                    srcy = CALC_SRCPOS_Y(y);

                    uint ii = srcy * bg->map_width + srcx;
                    uint oi = ((y & size_mod_mask) << 5) + (x & size_mod_mask);
                    uint entry = (uint) map_data[ii];
                    write_scr_block(entry, se16 + (oi << dst_shift));
                }
            }
        }

        // y scrolling
        if (cam_ty != prev_cam_ty)
        {
            int sy, ey;
            if (cam_ty > prev_cam_ty)
            {
                sy = prev_cam_ty + height_div + 1;
                ey = cam_ty + height_div;
            }
            else
            {
                sy = cam_ty == 0 ? 0 : cam_ty - 1;
                ey = prev_cam_ty == 0 ? 0 : prev_cam_ty - 1;
            }

            int sx = cam_tx;
            int ex = cam_tx + width_div;

            int srcx, srcy;
            for (int y = sy; y <= ey; ++y)
            {
                srcy = CALC_SRCPOS_Y(y);
                for (int x = sx; x <= ex; ++x)
                {
                    srcx = CALC_SRCPOS_X(x);

                    uint ii = srcy * bg->map_width + srcx;
                    uint oi = ((y & size_mod_mask) << 5) + (x & size_mod_mask);
                    uint entry = (uint) map_data[ii];
                    write_scr_block(entry, se16 + (oi << dst_shift));
                }
            }
        }
    }
}

IWRAM_CODE
static void write_scr_block16(const uint map_entry, u16 *p_dest)
{
    // dest should always be 32-bit aligned, since dst stride is 2
    u32 *dest = (u32 *)p_dest;

    if (map_entry == 0)
    {
        *dest        = 0;
        *(dest + 16) = 0;
        return;
    }

    uint gfx_id = (map_entry - 1) & 0xFF;
    uint v = gfx_id % 16 * 2 + gfx_id / 16 * 64;
    v = GFX_CHAR_GAME_TILESET + v + 1;

    const u32 se_flags = SE_PALBANK(GFX_BGPAL_BLACK_MUL);

    u32 upper = (((v+1) | se_flags) << 16) | (v | se_flags);
    v += 32;
    u32 lower = (((v+1) | se_flags) << 16) | (v | se_flags);

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
    *(dest + 16) = lower;
}

IWRAM_CODE
static void write_scr_block8(const uint map_entry, u16 *dest)
{
    *dest = (u16) map_entry;
}

static void update_map_scroll(uint bg_idx)
{
    gfx_bg_s *bg = gfx_ctl.bg + bg_idx;
    bg_scroll_data_s *scroll_data = bg_scroll_data + bg_idx;

    if (bg->map)
    {
        switch (bg->map->gfx_format)
        {
        case GFX_MAP_FORMAT_GBA:
            update_map_scroll_t(bg_idx, 3, 0, write_scr_block8);
            break;

        case GFX_MAP_FORMAT_MAPC16:
            update_map_scroll_t(bg_idx, 4, 1, write_scr_block16);
            break;

        case GFX_MAP_FORMAT_CUSTOM16:
        {
            update_map_scroll_t(bg_idx, 4, 1, bg->map->custom_write);
            break;
        }
        
        default:
            LOG_ERR("invalid map gfx format %u", bg->map->gfx_format);
            ASM_BREAK();
            break;
        }
    }

    scroll_data->old_offset_x = bg->offset_x;
    scroll_data->old_offset_y = bg->offset_y;
}

void gfx_mark_scroll_dirty(uint bg_idx)
{
    bg_scroll_data[bg_idx].screen_dirty = true;
}

void gfx_load_map(uint bg_idx, const gfx_map_s *map)
{
    gfx_bg_s *bg = gfx_ctl.bg + bg_idx;
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
EWRAM_BSS bool gfx_text_bmp_dirty_rows[GFX_TEXT_BMP_ROWS + 2];

ARM_FUNC void _gfx_text_blit_tile(uint x, uint y, const TILE4 *src_tile);

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

ARM_FUNC NO_INLINE
void gfx_text_bmap_fill_rect(uint x, uint y, uint width, uint height,
                             uint fg_col, uint bg_col)
{
    if (width == 0 || height == 0) return;

    uint col_s = x / 8;
    uint row_s = y / 8;
    uint col_e = (x + width) / 8;
    uint row_e = (y + height) / 8;
    uint cols = col_e - col_s + 1;
    uint rows = row_e - row_s + 1;

    // function expects all tiles to be used...
    if (cols < 3 || rows < 3)
    {
        LOG_ERR("gfx_text_bmap_fill_rect: given rectangle is too small...");
        DBG_BREAK();
        return;
    }

    struct
    {
        union
        {
            struct
            {
                u32 tl[8];
                u32 tr[8];
                u32 bl[8];
                u32 br[8];
            };
            struct
            {
                u32 t[8];
                u32 r[8];
                u32 l[8];
                u32 b[8];
            };
        };
        u32 body[8];
    } bmaps;

    // memset32(&bmaps, 0, sizeof(bmaps) / 4);

    // create 32-bit number that is filled with the fg color nibble
    u32 fg_fill = fg_col & 0xFF;
    fg_fill |= fg_fill << 4;
    fg_fill |= fg_fill << 8;
    fg_fill |= fg_fill << 16;

    // do the same with the bg color
    u32 bg_fill = bg_col & 0xFF;
    bg_fill |= bg_fill << 4;
    bg_fill |= bg_fill << 8;
    bg_fill |= bg_fill << 16;

    const u32 bmask_all = 0xFFFFFFFF;

    uint yofs_t = y & 7;
    uint yofs_b = (y + height) & 7;
    uint xofs_l = x & 7;
    uint xofs_r = (x + width) & 7;

    u32 mask_l, mask_r;
    u32 fill_l, fill_r;

    // compute fills for left and right side
    mask_l = bmask_all << (xofs_l * 4);
    fill_l = (fg_fill & mask_l) | (bg_fill & ~mask_l);

    mask_r = bmask_all >> (32 - xofs_r * 4);
    fill_r = (fg_fill & mask_r) | (bg_fill & ~mask_r);
    
    // fill body
    memset32(bmaps.body, fg_fill, 8);

    // perform tile fills:
    //   body
    gfx_text_bmap_fill(col_s + 1, row_s + 1, cols - 2, rows - 2, bmaps.body);

    //    edges
    // fill left and right bitmaps
    memset32(bmaps.l, fill_l, 8);
    memset32(bmaps.r, fill_r, 8);

    // fill top bitmap
    memset32(bmaps.t, bg_fill, yofs_t);
    memset32(bmaps.t + yofs_t, fg_fill, 8 - yofs_t);

    // fill bottom bitmap
    memset32(bmaps.b, fg_fill, yofs_b);
    memset32(bmaps.b + yofs_b, bg_fill, 8 - yofs_b);
    
    gfx_text_bmap_fill(col_s, row_s + 1, 1, rows - 2, bmaps.l);
    gfx_text_bmap_fill(col_e, row_s + 1, 1, rows - 2, bmaps.r);
    gfx_text_bmap_fill(col_s + 1, row_s, cols - 2, 1, bmaps.t);
    gfx_text_bmap_fill(col_s + 1, row_e, cols - 2, 1, bmaps.b);

    //   corners
    // fill top-left and top-right bitmaps
    memset32(bmaps.tl, bg_fill, yofs_t);
    memset32(bmaps.tr, bg_fill, yofs_t);

    memset32(bmaps.tl + yofs_t, fill_l, 8 - yofs_t);
    memset32(bmaps.tr + yofs_t, fill_r, 8 - yofs_t);

    // fill bottom-left and bottom-right bitmaps
    memset32(bmaps.bl, fill_l, yofs_b);
    memset32(bmaps.br, fill_r, yofs_b);

    memset32(bmaps.bl + yofs_b, bg_fill, 8 - yofs_b);
    memset32(bmaps.br + yofs_b, bg_fill, 8 - yofs_b);

    gfx_text_bmap_fill(col_s, row_s, 1, 1, bmaps.tl);
    gfx_text_bmap_fill(col_e, row_s, 1, 1, bmaps.tr);
    gfx_text_bmap_fill(col_s, row_e, 1, 1, bmaps.bl);
    gfx_text_bmap_fill(col_e, row_e, 1, 1, bmaps.br);
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

    bool is_white = false;
    u32 color_bits = 0;

    switch (text_color)
    {
    case TEXT_COLOR_WHITE:
        is_white = true;
        break;
    case TEXT_COLOR_BLUE:
        color_bits = 0x33333333;
        break;
    case TEXT_COLOR_YELLOW:
        color_bits = 0x22222222;
        break;
    case TEXT_COLOR_BLACK:
        color_bits = 0x11111111;
        break;
    case TEXT_COLOR_CLEAR:
        color_bits = 0x00000000;
        break;
    }
    
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
    gfx_ctl.palette_mul = FIX_ONE;
    last_palette_mul = FIX_ONE;

    gfx_ctl.bg_userpal_mul  = 0b01111111;
    gfx_ctl.obj_userpal_mul = 0b01111111;

    dma_cpypool_write = dma_cpypool;
    memcpy16(gfx_palette, gfx_palette_normal, 16);
    oam_init(gfx_oam_buffer, 128);
    reset_palette();

    for (uint i = 0; i < 4; ++i)
    {
        gfx_ctl.bg[i] = (gfx_bg_s)
        {
            .bpp = GFX_BG_4BPP,
            .enabled = false,
        };

        bg_scroll_data[i] = (bg_scroll_data_s){0};
    };

    gfx_ctl.bg[0].enabled = true;
    gfx_ctl.bg[0].char_block = GFX_TEXT_BMP_BLOCK;
    gfx_ctl.bg[0].priority = 0;
    gfx_ctl.bg[1].priority = 1;
    gfx_ctl.bg[2].priority = 2;
    gfx_ctl.bg[3].priority = 3;
    gfx_ctl.enable_obj = true;
}

void gfx_commit()
{
    if (gfx_ctl.palette_mul != last_palette_mul)
    {
        last_palette_mul = gfx_ctl.palette_mul;
        apply_palette_multiply(gfx_ctl.palette_mul);
    }

    // update bg palettes
    for (int p = 0; p < GFX_BGPAL_USER_COUNT; ++p)
    {
        u16 *pal = (gfx_ctl.bg_userpal_mul & (1 << p))
                   ? gfx_mul_palette : gfx_palette;
        for (int i = 0; i < 16; ++i)
        {
            pal_bg_bank[GFX_BGPAL_USER0 + p][i] =
                pal[gfx_ctl.bg_userpal[p][i]];
        }
    }

    // update obj palettes
    for (int p = 0; p < GFX_OBJPAL_USER_COUNT; ++p)
    {
        u16 *pal = (gfx_ctl.obj_userpal_mul & (1 << p))
                   ? gfx_mul_palette : gfx_palette;   
        for (int i = 0; i < 16; ++i)
        {
            pal_obj_bank[GFX_OBJPAL_USER0 + p][i] =
                pal[gfx_ctl.obj_userpal[p][i]];
        }
    }

    oam_copy(oam_mem, gfx_oam_buffer, 128);

    flush_dma_queue();

    uint text_dma_count = 0; // in bytes

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
        text_dma_count += sz;
        r0 = r1;
    }

#if false // #ifdef DEVDEBUG
    if (text_dma_count > 0)
        LOG_DBG("text dma copies: %u", text_dma_count);
#else
    (void)text_dma_count;
#endif

    memset(gfx_text_bmp_dirty_rows, 0, sizeof(gfx_text_bmp_dirty_rows));

    REG_BG0HOFS = gfx_ctl.bg[0].offset_x;
    REG_BG0VOFS = gfx_ctl.bg[0].offset_y;
    REG_BG1HOFS = gfx_ctl.bg[1].offset_x;
    REG_BG1VOFS = gfx_ctl.bg[1].offset_y;
    REG_BG2HOFS = gfx_ctl.bg[2].offset_x;
    REG_BG2VOFS = gfx_ctl.bg[2].offset_y;
    REG_BG3HOFS = gfx_ctl.bg[3].offset_x;
    REG_BG3VOFS = gfx_ctl.bg[3].offset_y;
    
    u32 reg_dispcnt = DCNT_OBJ_1D;
    u16 bg_cnt[4] = { 0, 0, 0, 0 };
    
    bg_cnt[0] = BG_SBB(GFX_BG0_INDEX) | BG_REG_32x32;
    bg_cnt[1] = BG_SBB(GFX_BG1_INDEX) | BG_REG_32x32;
    bg_cnt[2] = BG_SBB(GFX_BG2_INDEX) | BG_REG_32x32;
    bg_cnt[3] = BG_SBB(GFX_BG3_INDEX) | BG_REG_32x32;

    if (gfx_ctl.enable_obj)
        reg_dispcnt |= DCNT_OBJ;

    for (uint i = 0; i < 4; ++i)
    {
        gfx_bg_s *config = gfx_ctl.bg + i;
        if (config->enabled)
            reg_dispcnt |= DCNT_BG0 << i;

        u16 *const cnt = bg_cnt + i;

        *cnt |= BG_PRIO(config->priority) | BG_CBB(config->char_block);
        if (config->bpp == GFX_BG_8BPP)
            *cnt |= BG_8BPP;
        else
            *cnt |= BG_4BPP;
    }

    if (gfx_ctl.win[0].enabled)
    {
        reg_dispcnt |= DCNT_WIN0;
        REG_WIN0H = gfx_ctl.win[0].h;
        REG_WIN0V = gfx_ctl.win[0].v;
    }

    if (gfx_ctl.win[1].enabled)
    {
        reg_dispcnt |= DCNT_WIN1;
        REG_WIN1H = gfx_ctl.win[1].h;
        REG_WIN1V = gfx_ctl.win[1].v;
    }

    REG_WININ =   (gfx_ctl.bg[0].enable_win_in[0] << 0 )
                | (gfx_ctl.bg[1].enable_win_in[0] << 1 )
                | (gfx_ctl.bg[2].enable_win_in[0] << 2 )
                | (gfx_ctl.bg[3].enable_win_in[0] << 3 )
                | (gfx_ctl.bg[0].enable_win_in[1] << 8 )
                | (gfx_ctl.bg[1].enable_win_in[1] << 9 )
                | (gfx_ctl.bg[2].enable_win_in[1] << 10)
                | (gfx_ctl.bg[3].enable_win_in[1] << 11);

    REG_WINOUT =   (gfx_ctl.bg[0].enable_win_out << 0)
                 | (gfx_ctl.bg[1].enable_win_out << 1)
                 | (gfx_ctl.bg[2].enable_win_out << 2)
                 | (gfx_ctl.bg[3].enable_win_out << 3);

    REG_DISPCNT = reg_dispcnt;
    REG_BG0CNT = bg_cnt[0];
    REG_BG1CNT = bg_cnt[1];
    REG_BG2CNT = bg_cnt[2];
    REG_BG3CNT = bg_cnt[3];
}

void gfx_new_frame(void)
{
    for (uint i = 0; i < 4; ++i)
        update_map_scroll(i);

    gfx_commit();

#if defined(DEVDEBUG) && defined(PLATFORM_GBA)
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