#include <tonc.h>
#include <game_sprdb.h>
#include <tileset_gfx.h>
#include <assert.h>

#include "map_data.h"
#include "game.h"
#include "gfx.h"
#include "log.h"

// #define MAIN_PROFILE

int main()
{
    LOG_INIT();
    LOG_DBG("Hello, world!");

    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    gfx_init();
    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, game_sprdb_gfxTiles, game_sprdb_gfxTilesLen / sizeof(u32));

    const map_header_s *map = maps[1];
    gfx_load_map(map);
    game_init();
    game_load_room(map);

    int last_obj_index = 0;

    entity_s *player = entity_alloc();
    player->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR;
    player->pos.x = int2fx(16);
    player->pos.y = int2fx(16);
    player->col.w = 6;
    player->col.h = 8;
    player->actor.move_speed = (FIXED)(FIX_SCALE * 1);
    player->actor.move_accel = (FIXED)(FIX_SCALE / 8);
    player->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
    player->sprite.ox = -1;
    player->sprite.oy = -8;
    player->sprite.graphic_id = SPRID_GAME_PLAYER_WALK;

    // {
    //     entity_s *e = entity_alloc();
    //     e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
    //     e->pos.x = int2fx(32);
    //     e->pos.y = int2fx(32);
    //     e->col.w = 6;
    //     e->col.h = 8;
    //     e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
    //     e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
    //     e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
    //     e->sprite.ox = -1;
    //     e->sprite.oy = -8;
    //     e->mass = 2;
    // }

    // {
    //     entity_s *e = entity_alloc();
    //     e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
    //     e->pos.x = int2fx(32);
    //     e->pos.y = int2fx(64);
    //     e->col.w = 6;
    //     e->col.h = 8;
    //     e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
    //     e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
    //     e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
    //     e->sprite.ox = -1;
    //     e->sprite.oy = -8;
    //     e->mass = 4;
    // }

    while (true)
    {
        VBlankIntrWait();

        #ifdef MAIN_PROFILE
        profile_start();
        #endif

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

        if (key_hit(KEY_A))
        {
            entity_s *droplet = entity_alloc();
            if (droplet)
            {
                int dir = (int) player->actor.face_dir;
                entity_player_droplet_init(droplet,
                                           player->pos.x, player->pos.y,
                                           PLAYER_DROPLET_TYPE_SIDE, dir);
            }
        }

        // if (key_is_down(KEY_UP))
        //     player->pos.y -= int2fx(1);

        // if (key_is_down(KEY_DOWN))
        //     player->pos.y += int2fx(1);

        game_update();

        game_cam_x = (player->pos.x >> FIX_SHIFT) - SCREEN_WIDTH / 4;
        game_cam_y = (player->pos.y >> FIX_SHIFT) - SCREEN_HEIGHT / 4;

        int x_max = gfx_map_width * 8 - SCREEN_WIDTH / 2;
        int y_max = gfx_map_height * 8 - SCREEN_HEIGHT / 2;

        if (game_cam_x < 0)     game_cam_x = 0;
        if (game_cam_y < 0)     game_cam_y = 0;
        if (game_cam_x > x_max) game_cam_x = x_max;
        if (game_cam_y > y_max) game_cam_y = y_max;

        game_render(&last_obj_index);

        #ifdef MAIN_PROFILE
        uint frame_len = profile_stop();
        LOG_DBG("frame usage: %.1f%%", (float)frame_len / 280896.f * 100.f);
        #endif
    }


    return 0;
}
