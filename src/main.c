#include <tonc.h>
#include <player_gfx.h>
#include <tileset_gfx.h>
#include <assert.h>
#include "mgba.h"

#include "map_data.h"
#include "game.h"
#include "gfx.h"

int main()
{
    mgba_console_open();
    mgba_printf(MGBA_LOG_DEBUG, "Hello, world!");

    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    gfx_init();
    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, player_gfxTiles, 32 * 8);

    const map_header_s *map = maps[1];
    gfx_load_map(map);
    game_init();
    game_load_room(map);

    OBJ_ATTR *obj = &gfx_oam_buffer[0];
    obj_set_attr(obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK(0));
    int cam_x = 0;
    int cam_y = 0;

    entity_s *player = &game_entities[0];
    player->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR;
    player->pos.x = int2fx(16);
    player->pos.y = int2fx(16);
    player->col.w = 6;
    player->col.h = 8;
    player->actor.move_speed = (FIXED)(FIX_SCALE * 1);
    player->actor.move_accel = (FIXED)(FIX_SCALE / 8);
    player->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
    player->sprite.ox = -1;

    while (true)
    {
        VBlankIntrWait();
        gfx_new_frame();
        key_poll();

        int player_move_x = 0;

        if (key_is_down(KEY_RIGHT))
            ++player_move_x;

        if (key_is_down(KEY_LEFT))
            --player_move_x;

        player->actor.move_x = (s8) player_move_x;

        if (key_hit(KEY_B))
            player->actor.jump_trigger = 8;

        // if (key_is_down(KEY_UP))
        //     player->pos.y -= int2fx(1);

        // if (key_is_down(KEY_DOWN))
        //     player->pos.y += int2fx(1);

        game_update();

        int px = player->pos.x / FIX_SCALE + player->sprite.ox;
        int py = player->pos.y / FIX_SCALE + player->sprite.oy;
        cam_x = px - SCREEN_WIDTH / 4;
        cam_y = py - SCREEN_HEIGHT / 4;

        int x_max = gfx_map_width * 8 - SCREEN_WIDTH / 2;
        int y_max = gfx_map_height * 8 - SCREEN_HEIGHT / 2;

        if (cam_x < 0)     cam_x = 0;
        if (cam_y < 0)     cam_y = 0;
        if (cam_x > x_max) cam_x = x_max;
        if (cam_y > y_max) cam_y = y_max;

        gfx_scroll_x = cam_x * 2;
        gfx_scroll_y = cam_y * 2;

        obj_set_pos(obj, (px - cam_x) * 2, (py - cam_y) * 2);
    }


    return 0;
}
