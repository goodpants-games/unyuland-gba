#ifndef AUTOMAP_H
#define AUTOMAP_H

#include <world.h>

#define AUTOMAP_MARGIN_X 8
#define AUTOMAP_MARGIN_Y 8
#define AUTOMAP_WIDTH (WORLD_MATRIX_WIDTH * 2 + AUTOMAP_MARGIN_X * 2)
#define AUTOMAP_HEIGHT (WORLD_MATRIX_HEIGHT * 2 + AUTOMAP_MARGIN_Y * 2)
#define AUTOMAP_GFX_SCALE 2
#define AUTOMAP_EMPTY 0xFF

typedef struct automap
{
    u8 map[AUTOMAP_HEIGHT][AUTOMAP_WIDTH];
    map_header_s scrmap_header;

    // TODO: maybe make this a 16x16 scroll map, but extend gfx capabilities to
    //       allow a custom write_scr_block function. i can't use the regular
    //       16x16 mode because, without duplicate pruning, there will be too
    //       many tiles for the vram screenblock.
    u16 scrmap[AUTOMAP_HEIGHT * AUTOMAP_GFX_SCALE]
              [AUTOMAP_WIDTH  * AUTOMAP_GFX_SCALE];
}
automap_s;

void automap_init(automap_s *map);

#endif