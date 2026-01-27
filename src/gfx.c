#include <tonc.h>
#include "gfx.h"
#include "mgba.h"

OBJ_ATTR gfx_oam_buffer[128];
int gfx_scroll_x = 0;
int gfx_scroll_y = 0;
uint gfx_map_width = 0;
uint gfx_map_height = 0;
const map_header_s *gfx_loaded_map = NULL;

static uint old_scroll_x = 0;
static uint old_scroll_y = 0;
static bool screen_dirty = false;

static void write_scr_block(const uint map_entry, u32 *const dest)
{
    int gfx_id = map_entry & 0xFF;
    int v = gfx_id % 16 * 2 + gfx_id / 16 * 64;

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

    pal_bg_mem[ 0] = 0x0000;
    pal_bg_mem[ 1] = 0x28a3;
    pal_bg_mem[ 2] = 0x288f;
    pal_bg_mem[ 3] = 0x2a00;
    pal_bg_mem[ 4] = 0x1955;
    pal_bg_mem[ 5] = 0x254b;
    pal_bg_mem[ 6] = 0x6318;
    pal_bg_mem[ 7] = 0x77df;
    pal_bg_mem[ 8] = 0x241f;
    pal_bg_mem[ 9] = 0x029f;
    pal_bg_mem[10] = 0x13bf;
    pal_bg_mem[11] = 0x1b80;
    pal_bg_mem[12] = 0x7ea5;
    pal_bg_mem[13] = 0x4dd0;
    pal_bg_mem[14] = 0x55df;
    pal_bg_mem[15] = 0x573f;

    pal_obj_mem[ 0] = 0x0000;
    pal_obj_mem[ 1] = 0x28a3;
    pal_obj_mem[ 2] = 0x288f;
    pal_obj_mem[ 3] = 0x2a00;
    pal_obj_mem[ 4] = 0x1955;
    pal_obj_mem[ 5] = 0x254b;
    pal_obj_mem[ 6] = 0x6318;
    pal_obj_mem[ 7] = 0x77df;
    pal_obj_mem[ 8] = 0x241f;
    pal_obj_mem[ 9] = 0x029f;
    pal_obj_mem[10] = 0x13bf;
    pal_obj_mem[11] = 0x1b80;
    pal_obj_mem[12] = 0x7ea5;
    pal_obj_mem[13] = 0x4dd0;
    pal_obj_mem[14] = 0x55df;
    pal_obj_mem[15] = 0x573f;

    REG_BG0CNT = BG_CBB(0) | BG_SBB(GFX_BG0_INDEX) | BG_4BPP | BG_REG_32x32;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_DISPCNT = DCNT_OBJ | DCNT_OBJ_1D | DCNT_BG0;
}

void gfx_new_frame(void)
{   
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
            int ey = cam_ty + SCRH_T16;
            int ex = cam_tx + SCRW_T16;

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
                    sx = prev_cam_tx + SCRW_T16;
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
                    sy = prev_cam_ty + SCRH_T16;
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