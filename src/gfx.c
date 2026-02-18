#include <tonc.h>
#include "gfx.h"
#include "mgba.h"
#include "log.h"

OBJ_ATTR gfx_oam_buffer[128];
int gfx_scroll_x = 0;
int gfx_scroll_y = 0;
uint gfx_map_width = 0;
uint gfx_map_height = 0;
const map_header_s *gfx_loaded_map = NULL;

static uint old_scroll_x = 0;
static uint old_scroll_y = 0;
static bool screen_dirty = false;

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

static void write_scr_block(const uint map_entry, u32 *const dest)
{
    if (map_entry == 0)
    {
        *dest = 0;
        *(dest + 16) = 0;
        return;
    }

    int gfx_id = map_entry & 0xFF;
    int v = gfx_id % 16 * 2 + gfx_id / 16 * 64;
    --v;

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

    REG_BG0CNT = BG_CBB(0) | BG_SBB(GFX_BG0_INDEX) | BG_8BPP | BG_REG_32x32;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_DISPCNT = DCNT_OBJ_1D;
}

void gfx_new_frame(void)
{
    REG_DISPCNT |= DCNT_OBJ | DCNT_BG0;

    if (gfx_loaded_map)
    {
        const u16 *map_data = map_graphics_data(gfx_loaded_map);
        u32 *se32 = (u32 *)se_mem[GFX_BG0_INDEX];

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
    REG_BG0HOFS = gfx_scroll_x;
    REG_BG0VOFS = gfx_scroll_y;
}

void gfx_load_map(const map_header_s *map)
{
    gfx_loaded_map = map;
    gfx_map_width = gfx_loaded_map->width;
    gfx_map_height = gfx_loaded_map->height;
    screen_dirty = true;
}

void gfx_reset_palette(void)
{
    // 0-15: regular palette, may be darkened for room transition
    // 16: black (if on 16-bit color mode, this will be the start of the 2nd bank)
    // 17-32: regular palette + black, will not be darkened during room trans
    for (int i = 0; i < 16; ++i)
        pal_bg_mem[i] = gfx_palette[i];
    pal_bg_mem[16] = gfx_palette[0];

    for (int i = 1; i < 16; ++i)
        pal_bg_mem[16 + i] = gfx_palette[i];
    pal_bg_mem[32] = gfx_palette[0];

    // pal bank 0: regular palette, may be darkened for room transition
    // pal bank 1: dynamically changing rainbow palette (TODO)
    for (int i = 0; i < 16; ++i)
        pal_obj_bank[0][i] = gfx_palette[i];
}

void gfx_set_palette_multiplied(FIXED factor)
{
    if      (factor < 0)       factor = 0;
    else if (factor > FIX_ONE) factor = FIX_ONE;

    u16 new_palette[16];
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

        new_palette[i] = gfx_palette[color_index];
    }

    for (int i = 1; i < 16; ++i)
        pal_bg_mem[i] = new_palette[i];

    for (int i = 1; i < 16; ++i)
        pal_obj_bank[0][i] = new_palette[i];
}