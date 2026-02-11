#include <tonc.h>
#include <game_sprdb.h>
#include <tileset_gfx.h>
#include <assert.h>

#include <world.h>
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
    memcpy32(&tile_mem[0][0] + 2, tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, game_sprdb_gfxTiles, game_sprdb_gfxTilesLen / sizeof(u32));

    const map_header_s *map = world_rooms[0];
    gfx_load_map(map);
    game_init();
    game_load_room(map);

    int last_obj_index = 0;

    entity_s *player = entity_alloc();
    entity_player_init(player);
    player->pos.x = int2fx(16);
    player->pos.y = int2fx(16);

    LOG_DBG("room pos: %i %i", (int) map->px, (int) map->py);

    if (false)
    {
        entity_s *e = entity_alloc();
        e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
        e->pos.x = int2fx(32);
        e->pos.y = int2fx(32);
        e->col.w = 6;
        e->col.h = 8;
        e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
        e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
        e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
        e->sprite.ox = -1;
        e->sprite.oy = -8;
        e->mass = 2;
    }

    if (false)
    {
        entity_s *e = entity_alloc();
        e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
        e->pos.x = int2fx(32);
        e->pos.y = int2fx(64);
        e->col.w = 6;
        e->col.h = 8;
        e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
        e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
        e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
        e->sprite.ox = -1;
        e->sprite.oy = -8;
        e->mass = 4;
    }

    while (true)
    {
        #ifdef MAIN_PROFILE
        profile_start();
        #endif

        key_poll();
        game_update();
        game_transition_update(player);

        g_game.cam_x = (player->pos.x >> FIX_SHIFT) - SCREEN_WIDTH / 4;
        g_game.cam_y = (player->pos.y >> FIX_SHIFT) - SCREEN_HEIGHT / 4;

        int x_max = gfx_map_width * 8 - SCREEN_WIDTH / 2;
        int y_max = gfx_map_height * 8 - SCREEN_HEIGHT / 2;

        if (g_game.cam_x < 0)     g_game.cam_x = 0;
        if (g_game.cam_y < 0)     g_game.cam_y = 0;
        if (g_game.cam_x > x_max) g_game.cam_x = x_max;
        if (g_game.cam_y > y_max) g_game.cam_y = y_max;

        game_render(&last_obj_index);

        #ifdef MAIN_PROFILE
        uint frame_len = profile_stop();
        LOG_DBG("frame usage: %.1f%%", (float)frame_len / 280896.f * 100.f);
        #endif

        VBlankIntrWait();
        gfx_new_frame();
    }


    return 0;
}
