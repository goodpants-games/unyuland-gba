#ifndef AUTOMAP_H
#define AUTOMAP_H

#include <world.h>

#define MAP_SCREEN_WIDTH     (15 * 8)
#define MAP_SCREEN_HEIGHT    (11 * 8)
#define AUTOMAP_MARGIN_X     4
#define AUTOMAP_MARGIN_Y     4
#define AUTOMAP_SCALE        2
#define AUTOMAP_INNER_WIDTH  (WORLD_MATRIX_WIDTH * AUTOMAP_SCALE)
#define AUTOMAP_INNER_HEIGHT (WORLD_MATRIX_HEIGHT * AUTOMAP_SCALE)
#define AUTOMAP_WIDTH        (AUTOMAP_INNER_WIDTH + AUTOMAP_MARGIN_X * 2)
#define AUTOMAP_HEIGHT       (AUTOMAP_INNER_HEIGHT + AUTOMAP_MARGIN_Y * 2)

typedef struct automap
{
    bool is_open;
    u8 frame;
    u8 view_bg_idx;

    int player_x;
    int player_y;
    FIXED sx, sy;
    const world_room_s *cur_room;

    map_header_s scrmap_header;
    u16 scrmap[AUTOMAP_HEIGHT][AUTOMAP_WIDTH];
    bool visited[AUTOMAP_INNER_HEIGHT][AUTOMAP_INNER_WIDTH];
}
automap_s;

void automap_init(automap_s *map);
void automap_open_view(automap_s *map, int bg_idx);
void automap_close_view(automap_s *map);
void automap_set_pos(automap_s *map, const world_room_s *room, int local_x,
                     int local_y);
void automap_update_view(automap_s *map);

#endif