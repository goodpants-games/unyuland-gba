#ifndef AUTOMAP_H
#define AUTOMAP_H

#include <world.h>

#define AUTOMAP_MARGIN_X 4
#define AUTOMAP_MARGIN_Y 4
#define AUTOMAP_WIDTH (WORLD_MATRIX_WIDTH * 2 + AUTOMAP_MARGIN_X * 2)
#define AUTOMAP_HEIGHT (WORLD_MATRIX_HEIGHT * 2 + AUTOMAP_MARGIN_Y * 2)

typedef struct automap
{
    u8 map[AUTOMAP_HEIGHT][AUTOMAP_WIDTH];

    map_header_s scrmap_header;
    u16 scrmap[AUTOMAP_HEIGHT][AUTOMAP_WIDTH];
}
automap_s;

void automap_init(automap_s *map);

#endif