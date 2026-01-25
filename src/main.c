#include <tonc.h>
#include <player_gfx.h>
#include <tileset_gfx.h>

#include "maps.h"
#include "mgba.h"

#define SCRW_T16 (SCREEN_WIDTH / 16)
#define SCRH_T16 (SCREEN_HEIGHT / 16)

OBJ_ATTR obj_buffer[128];

static void write_block(const uint map_entry, u32 *const dest)
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

int main()
{
    mgba_console_open();

    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    // set up graphics state
    oam_init(obj_buffer, 128);

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

    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, player_gfxTiles, 32 * 8);

    const map_header_s *map = maps[0];
    int map_width = (int) map->width;
    int map_height = (int) map->height;

    mgba_printf(MGBA_LOG_INFO, "map size: %d, %d", map_width, map_height);

    const u16 *map_data = map_graphics_data(map);

    u32 *se32 = (u32 *)se_mem[28];
    // screen size: 15x10 tiles
    
    for (int y = 0; y < 10; ++y)
    {
        for (int x = 0; x < 15; ++x)
        {
            uint ii = y * map_width + x;
            uint oi = ((y << 5) | x);
            uint entry = (uint) map_data[ii];
            write_block(entry, se32 + oi);
        }
    }

    REG_BG0CNT = BG_CBB(0) | BG_SBB(28) | BG_4BPP | BG_REG_32x32;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_DISPCNT = DCNT_OBJ | DCNT_OBJ_1D | DCNT_BG0;

    OBJ_ATTR *obj = &obj_buffer[0];
    obj_set_attr(obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK(0));

    int x = 96 * 2;
    int y = 32 * 2;
    int cam_x = 0;
    int cam_y = 0;
    int prev_cam_x = cam_x;
    int prev_cam_y = cam_y;

    while (true)
    {
        VBlankIntrWait();

        int prev_cam_tx = prev_cam_x / 16;
        int cam_tx = cam_x / 16;

        int prev_cam_ty = prev_cam_y / 16;
        int cam_ty = cam_y / 16;

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
                    uint ii = y * map_width + x;
                    uint oi = (y % 16) * 32 + (x % 16);
                    uint entry = (uint) map_data[ii];
                    write_block(entry, se32 + oi);
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
                    uint ii = y * map_width + x;
                    uint oi = (y % 16) * 32 + (x % 16);
                    uint entry = (uint) map_data[ii];
                    write_block(entry, se32 + oi);
                }
            }
        }

        prev_cam_x = cam_x;
        prev_cam_y = cam_y;

        obj_set_pos(obj, x / 2, y / 2);
        oam_copy(oam_mem, obj_buffer, 1);
        REG_BG0HOFS = cam_x;
        REG_BG0VOFS = cam_y;

        key_poll();

        if (key_is_down(KEY_RIGHT))
            cam_x += 4;

        if (key_is_down(KEY_LEFT))
            cam_x -= 4;

        if (key_is_down(KEY_UP))
            cam_y -= 4;

        if (key_is_down(KEY_DOWN))
            cam_y += 4;

        if (cam_x < 0) cam_x = 0;
        if (cam_y < 0) cam_y = 0;
    }


    return 0;
}
