#include <tonc.h>
#include <ctype.h>
#include <font_gfx.h>
#include "gfx.h"
#include "gba_util.h"

#define TEXT_CHAR_ID(i) ((i) * 4)

OBJ_ATTR gfx_oam_buffer[128];
int gfx_scroll_x = 0;
int gfx_scroll_y = 0;
uint gfx_map_width = 0;
uint gfx_map_height = 0;
const map_header_s *gfx_loaded_map = NULL;

static u16 gfx_mul_palette[16];

static uint old_scroll_x = 0;
static uint old_scroll_y = 0;
static bool screen_dirty = false;

#define RAINBOW_PALETTE_LENGTH (sizeof(rainbow_pal) / sizeof(*rainbow_pal))

static const int rainbow_pal[] = {
    GFX_PAL_RED, GFX_PAL_ORANGE, GFX_PAL_YELLOW, GFX_PAL_GREEN,
    GFX_PAL_BLUE, GFX_PAL_PINK
};

static int rainbow_shift = 0;
static int rainbow_shift_time_accum = 0;

u16 gfx_palette[16] = {
    0x0000,
    0x28a3,
    0x288f,
    0x2a00,
    0x1955,
    0x254b,
    0x6318,
    0x77df,
    0x241f,
    0x029f,
    0x13bf,
    0x1b80,
    0x7ea5,
    0x4dd0,
    0x55df,
    0x573f,
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

void gfx_init(void)
{
    oam_init(gfx_oam_buffer, 128);
    gfx_reset_palette();

    REG_DISPCNT = DCNT_OBJ_1D;

    REG_BG0CNT = BG_CBB(GFX_TEXT_BMP_BLOCK) | BG_SBB(GFX_BG0_INDEX) | BG_4BPP |
                 BG_REG_32x32 | BG_PRIO(0);
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;

    REG_BG1CNT = BG_CBB(0) | BG_SBB(GFX_BG1_INDEX) | BG_8BPP | BG_REG_32x32 |
                 BG_PRIO(1);
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;

    REG_BG2CNT = BG_SBB(GFX_BG2_INDEX) | BG_4BPP | BG_PRIO(2);
    REG_BG3CNT = BG_SBB(GFX_BG2_INDEX) | BG_4BPP | BG_PRIO(2);
}

void gfx_new_frame(void)
{
    REG_DISPCNT |= DCNT_OBJ | DCNT_BG0 | DCNT_BG1;

    if (++rainbow_shift_time_accum == 4)
    {
        rainbow_shift_time_accum = 0;
        if (++rainbow_shift == RAINBOW_PALETTE_LENGTH) rainbow_shift = 0;
        update_rainbow_palette();
    }

    if (gfx_loaded_map)
    {
        const u16 *map_data = map_graphics_data(gfx_loaded_map);
        u32 *se32 = (u32 *)se_mem[GFX_BG1_INDEX];

        int prev_cam_tx = old_scroll_x / 16;
        int cam_tx = gfx_scroll_x / 16;

        int prev_cam_ty = old_scroll_y / 16;
        int cam_ty = gfx_scroll_y / 16;

        if (screen_dirty)
        {
            screen_dirty = false;
            int ey = cam_ty + SCRH_T16 + 1;
            int ex = cam_tx + SCRW_T16 + 1;

            for (int y = cam_ty; y < ey; ++y)
            {
                for (int x = cam_tx; x < ex; ++x)
                {
                    uint ii = y * gfx_map_width + x;
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
                        uint ii = y * gfx_map_width + x;
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
                        uint ii = y * gfx_map_width + x;
                        uint oi = (y % 16) * 32 + (x % 16);
                        uint entry = (uint) map_data[ii];
                        write_scr_block(entry, se32 + oi);
                    }
                }
            }
        }
    }

    old_scroll_x = gfx_scroll_x;
    old_scroll_y = gfx_scroll_y;

    oam_copy(oam_mem, gfx_oam_buffer, 128);
    REG_BG1HOFS = gfx_scroll_x;
    REG_BG1VOFS = gfx_scroll_y;
}

void gfx_mark_scroll_dirty()
{
    screen_dirty = true;
}

void gfx_load_map(const map_header_s *map)
{
    gfx_loaded_map = map;
    gfx_map_width = gfx_loaded_map->width;
    gfx_map_height = gfx_loaded_map->height;
    screen_dirty = true;
}

// static inline u32* get_pixel_row(const int x, const int y)
// {
//     return &gfx_text_bmp[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[y & 7];
// }

static inline void blit_tile(uint x, uint y, const TILE4 *src_tile)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &GFX_TEXT_BMP_VRAM[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
    const u32 *src_row = src_tile->data;
    
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
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
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
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
        }
    }
}

static inline void blit_tile_colored(uint x, uint y, const TILE4 *src_tile,
                              u32 color_bits)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &GFX_TEXT_BMP_VRAM[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
    const u32 *src_row = src_tile->data;
    
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
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
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
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
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
void gfx_text_bmap_print(uint x, uint y, const char *text, text_color_e text_color)
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
            blit_tile(x, y, src_tile);
            blit_tile(x + 8, y, ++src_tile);
            blit_tile(x, y + 8, ++src_tile);
            blit_tile(x + 8, y + 8, ++src_tile);
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

// void gfx_text_bmap_fill(int oc, int or, int cols, int rows, u32 data[8])
// {
//     for (int r = or; r < or + rows; ++r)
//     {
//         TILE *t = &GFX_TEXT_BMP_VRAM[r * GFX_TEXT_BMP_COLS + oc];
//         TILE *end_tile = t + cols;
//         for (; t != end_tile; ++t)
//         {
//             for (int i = 0; i < 8; ++i)
//                 t->data[i] = data[i];
//         }
//     }
// }

void gfx_text_bmap_fill(uint oc, uint or, uint cols, uint rows, u32 data[8])
{
    for (uint r = or; r < or + rows; ++r)
    {
        TILE *t = &GFX_TEXT_BMP_VRAM[r * GFX_TEXT_BMP_COLS + oc];
        for (uint c = 0; c < cols; ++c, ++t)
        {
            for (uint i = 0; i < 8; ++i)
                t->data[i] = data[i];
        }
    }
}

void gfx_text_bmap_dst_clear(uint row, uint row_count)
{
    uint i = row * 32;
    for (uint y = row; y < row + row_count; ++y)
    {
        for (uint x = 0; x < 30; ++x)
            se_mem[GFX_BG0_INDEX][i++] = 0;
        i += 2;
    }
}

void gfx_text_bmap_dst_assign(uint row, uint row_count, uint src_row, uint pal)
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