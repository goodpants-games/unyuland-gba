#include <string.h>
#include <tonc_video.h>

#include <automap_tiles_gfx.h>
#include <automap_bin.h>

#include "automap.h"
#include "world.h"
#include "log.h"

#define TILE_STRIDE 99

static void scrblock_write(uint map_entry, u16 *dest)
{
    const u32 *src32 = (const u32 *)(automap_tiles_gfxMap + (map_entry * 2));
    u32 *dest32 = (u32 *)dest; // should be aligned as dst stride is 2

    *dest32 = *src32;
    *(dest32 + 16) = *(src32 + TILE_STRIDE);
}

void automap_init(automap_s *map)
{
    map->scrmap_header = (map_header_s)
    {
        .gfx_format = MAP_GFX_FORMAT_CUSTOM16,
        .width = AUTOMAP_WIDTH,
        .height = AUTOMAP_HEIGHT,
        .custom_scrblock_write = scrblock_write,
        .gfx_data_offset = offsetof(automap_s, scrmap)
                           - offsetof(automap_s, scrmap_header)
    };

    for (uint y = 0; y < AUTOMAP_HEIGHT; ++y)
    {
        bool yodd = y & 1;
        for (uint x = 0; x < AUTOMAP_WIDTH; ++x)
        {
            bool xodd = x & 1;

            u16 v;
            if (yodd & xodd)
                v = 0;
            else if (yodd)
                v = 3;
            else if (xodd)
                v = 2;
            else
                v = 1;

            map->scrmap[y][x] = v;
        }
    }

    // const u8 *am_data = (const u8 *)automap_bin;
    // const uint data_row_stride = WORLD_MATRIX_WIDTH * 2;

    // uint dst_y = AUTOMAP_MARGIN_Y;
    // for (uint y = 0; y < AUTOMAP_HEIGHT - AUTOMAP_MARGIN_Y * 2; ++y, ++dst_y)
    // {
    //     uint dst_x = AUTOMAP_MARGIN_X;
    //     for (uint x = 0; x < AUTOMAP_WIDTH - AUTOMAP_MARGIN_X * 2; ++x, ++dst_x)
    //     {
    //         u8 v = am_data[y * data_row_stride + x];
    //         if (v == 0xFF) continue;

    //         map->scrmap[dst_y][dst_x] = v;
    //     }
    // }
}

void automap_visit(automap_s *map, const world_room_s *room, int local_x,
                   int local_y)
{
    const int view_we = SCREEN_WIDTH / 3;
    const int view_he = SCREEN_HEIGHT / 3;

    const int room_x = (int) room->x;
    const int room_y = (int) room->y;
    const int room_w = ((int) room->map->width) * 8;
    const int room_h = ((int) room->map->height) * 8;

    // 16x16 px = 1 screen
    map->player_x =
        ((room_x * MAP_SCREEN_WIDTH + local_x)) * 16 / MAP_SCREEN_WIDTH
        + AUTOMAP_MARGIN_X * 8;
    map->player_y =
        ((room_y * MAP_SCREEN_HEIGHT + local_y)) * 16 / MAP_SCREEN_HEIGHT
        + AUTOMAP_MARGIN_Y * 8;

    LOG_DBG("playerpos (%i, %i)", map->player_x, map->player_y);

    int view_l = iclamp(local_x - view_we, 0, room_w);
    int view_r = iclamp(local_x + view_we, 0, room_w);
    int view_u = iclamp(local_y - view_he, 0, room_h);
    int view_d = iclamp(local_y + view_he, 0, room_h);

    int map_l = (view_l * AUTOMAP_SCALE) / MAP_SCREEN_WIDTH
                + room_x * AUTOMAP_SCALE;

    int map_r = (view_r * AUTOMAP_SCALE) / MAP_SCREEN_WIDTH
                + room_x * AUTOMAP_SCALE;

    int map_u = (view_u * AUTOMAP_SCALE) / MAP_SCREEN_HEIGHT
                + room_y * AUTOMAP_SCALE;

    int map_d = (view_d * AUTOMAP_SCALE) / MAP_SCREEN_HEIGHT
                + room_y * AUTOMAP_SCALE;

    const u8 *am_data = (const u8 *)automap_bin;
    const uint data_row_stride = WORLD_MATRIX_WIDTH * 2;

    for (int y = map_u; y < map_d; ++y)
    {
        int dst_y = y + AUTOMAP_MARGIN_Y;
        for (int x = map_l; x < map_r; ++x)
        {
            int dst_x = x + AUTOMAP_MARGIN_X;

            u8 v = am_data[y * data_row_stride + x];
            if (v == 0xFF) continue;

            map->scrmap[dst_y][dst_x] = v;
        }
    }
}