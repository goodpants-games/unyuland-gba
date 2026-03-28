#include "automap.h"
#include "world.h"

#include <automap_tiles_gfx.h>
#include <automap_bin.h>

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

    const u8 *am_data = (const u8 *)automap_bin;
    const uint data_row_stride = WORLD_MATRIX_WIDTH * 2;

    uint dst_y = AUTOMAP_MARGIN_Y;
    for (uint y = 0; y < AUTOMAP_HEIGHT - AUTOMAP_MARGIN_Y * 2; ++y, ++dst_y)
    {
        uint dst_x = AUTOMAP_MARGIN_X;
        for (uint x = 0; x < AUTOMAP_WIDTH - AUTOMAP_MARGIN_X * 2; ++x, ++dst_x)
        {
            u8 v = am_data[y * data_row_stride + x];
            if (v == 0xFF) continue;

            map->scrmap[dst_y][dst_x] = v;
        }
    }
}