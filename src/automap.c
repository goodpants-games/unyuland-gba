#include "automap.h"

#include <automap_tiles_gfx.h>

#define TILE_STRIDE 94

void automap_init(automap_s *map)
{
    uint gfx_scale = AUTOMAP_GFX_SCALE;

    map->scrmap_header = (map_header_s)
    {
        .gfx_format = MAP_GFX_FORMAT_GBA,
        .width = AUTOMAP_WIDTH * gfx_scale,
        .height = AUTOMAP_HEIGHT * gfx_scale,
        .gfx_data_offset = offsetof(automap_s, scrmap)
                           - offsetof(automap_s, scrmap_header)
    };

    for (uint y = AUTOMAP_MARGIN_Y; y < AUTOMAP_HEIGHT - AUTOMAP_MARGIN_Y; ++y)
    {
        for (uint x = AUTOMAP_MARGIN_X; x < AUTOMAP_WIDTH - AUTOMAP_MARGIN_X; ++x)
        {
            uint src_y = (y - AUTOMAP_MARGIN_Y) / 2;
            uint src_x = (x - AUTOMAP_MARGIN_X) / 2;

            bool has_level = world_matrix[src_y][src_x] != 0;
            u16 t = has_level ? 6 : 1;

            map->scrmap[(y * gfx_scale)][(x * gfx_scale)] = t;
            map->scrmap[(y * gfx_scale)][(x * gfx_scale) + 1] = t;
            map->scrmap[(y * gfx_scale) + 1][(x * gfx_scale)] = t;
            map->scrmap[(y * gfx_scale) + 1][(x * gfx_scale) + 1] = t;
        }
    }
}