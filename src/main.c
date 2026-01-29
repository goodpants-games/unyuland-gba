#include <tonc.h>
#include <game_sprites_gfx.h>
#include <tileset_gfx.h>
#include <assert.h>

#include "map_data.h"
#include "game.h"
#include "gfx.h"
#include "log.h"

int main()
{
    LOG_INIT();
    LOG_DBG("Hello, world!");

    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    gfx_init();
    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, game_sprites_gfxTiles, game_sprites_gfxTilesLen / sizeof(u32));

    const map_header_s *map = maps[1];
    gfx_load_map(map);
    game_init();
    game_load_room(map);

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

    {
        entity_s *e = &game_entities[1];
        e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
        e->pos.x = int2fx(32);
        e->pos.y = int2fx(32);
        e->col.w = 6;
        e->col.h = 8;
        e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
        e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
        e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
        e->sprite.ox = -1;
        e->mass = 2;
    }

    {
        entity_s *e = &game_entities[2];
        e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
        e->pos.x = int2fx(32);
        e->pos.y = int2fx(64);
        e->col.w = 6;
        e->col.h = 8;
        e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
        e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
        e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
        e->sprite.ox = -1;
        e->mass = 4;
    }

    while (true)
    {
        VBlankIntrWait();

        profile_start();

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

        cam_x = (player->pos.x >> FIX_SHIFT) - SCREEN_WIDTH / 4;
        cam_y = (player->pos.y >> FIX_SHIFT) - SCREEN_HEIGHT / 4;

        int x_max = gfx_map_width * 8 - SCREEN_WIDTH / 2;
        int y_max = gfx_map_height * 8 - SCREEN_HEIGHT / 2;

        if (cam_x < 0)     cam_x = 0;
        if (cam_y < 0)     cam_y = 0;
        if (cam_x > x_max) cam_x = x_max;
        if (cam_y > y_max) cam_y = y_max;

        gfx_scroll_x = cam_x * 2;
        gfx_scroll_y = cam_y * 2;

        int obj_index = 0;
        for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
        {
            const entity_s *ent = &game_entities[i];
            if (!(ent->flags & ENTITY_FLAG_ENABLED)) continue;

            int draw_x = (ent->pos.x >> FIX_SHIFT) + ent->sprite.ox;
            int draw_y = (ent->pos.y >> FIX_SHIFT) + ent->sprite.oy;

            obj_set_attr(&gfx_oam_buffer[obj_index], ATTR0_SQUARE,
                         ATTR1_SIZE_16, ATTR2_PALBANK(0));
            obj_set_pos(&gfx_oam_buffer[obj_index], (draw_x - cam_x) * 2,
                        (draw_y - cam_y) * 2);
            if (++obj_index >= 64) break;
        }

        uint frame_len = profile_stop();

        LOG_DBG("frame usage: %.1f%%", (float)frame_len / 280896.f * 100.f);
    }


    return 0;
}
