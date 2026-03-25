#include "automap.h"

#include <automap_tiles_gfx.h>

#define TILE_STRIDE 94

void automap_init(automap_s *map)
{
    map->scrmap_header.width = AUTOMAP_WIDTH;
    map->scrmap_header.height = AUTOMAP_HEIGHT;
    map->scrmap_header.gfx_data_offset = offsetof(automap_s, scrmap);

    for (int y = 0; y < AUTOMAP_HEIGHT; ++y)
    {
        for (int x = 0; x < AUTOMAP_WIDTH; ++x)
        {
            map->scrmap[y][x] = 4;
        }
    }
}