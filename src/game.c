#include <stdlib.h>
#include <tonc.h>
#include <world.h>

#include "game.h"
#include "game_physics.h"
#include "log.h"
#include "gfx.h"
#include "math_util.h"

#define GRAVITY TO_FIXED(0.1)
#define MAX_RENDER_OBJS ((MAX_ENTITY_COUNT + MAX_PROJECTILE_COUNT))
#define PROJ_FREE_QUEUE_MAX_SIZE 16

typedef enum render_obj_t
{
    RENDER_OBJ_SPRITE,
    RENDER_OBJ_PROJECTILE
} render_obj_t_e;

typedef struct gfx_frame
{
    u16 obj_pool_index;
    u8 width; // in tiles
    u8 height; // in tiles
    u8 frame_len;
    u8 obj_count;
} gfx_frame_s;

typedef struct gfx_sprite
{
    u8 frame_count;
    u8 loop; // bool
    u16 frame_pool_idx;
} gfx_sprite_s;

typedef struct gfx_obj {
    u16 a0; // contains only config for the sprite shape
    u16 a1; // contains only config for the sprite size
    u16 a2; // contains only config for the character index
    s8 ox;
    s8 oy;
    s8 flipped_ox;
    s8 flipped_oy;
} gfx_obj_s;

typedef struct gfx_root_header {
    uintptr_t  frame_pool;
    uintptr_t  obj_pool;
    
    gfx_sprite_s sprite0;
} gfx_root_header_s;

typedef struct render_obj
{
    u8 type;
    void *item;
}
render_obj_s;

game_s g_game;

static uint render_object_count = 0;
static render_obj_s render_objects[MAX_RENDER_OBJS];

static int proj_free_queue_count = 0;
static projectile_s *proj_free_queue[PROJ_FREE_QUEUE_MAX_SIZE];

entity_s* entity_alloc(void)
{
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *ent = g_game.entities + i;
        if (ent->flags & ENTITY_FLAG_ENABLED) continue;

        *ent = (entity_s) {
            .flags = ENTITY_FLAG_ENABLED,
            .gmult = FIX_ONE,
            .mass = 2,
            .actor.face_dir = 1,
            .col.group = COLGROUP_DEFAULT,
            .col.mask = COLGROUP_ALL
        };

        if (render_object_count == MAX_RENDER_OBJS)
            LOG_ERR("render object pool is full!");
        else
            render_objects[render_object_count++] = (render_obj_s)
            {
                .type = RENDER_OBJ_SPRITE,
                .item = ent
            };

        return ent;
    }

    LOG_ERR("entity pool is full!");
    return NULL;
}

void entity_free(entity_s *entity)
{
    // given entity is already free, don't do anything
    if (!(entity->flags & ENTITY_FLAG_ENABLED)) return;

    if (entity->behavior && entity->behavior->free)
        entity->behavior->free(entity);

    entity->flags = 0;

    // remove from render list
    for (int i = 0; i < render_object_count; ++i)
    {
        if (render_objects[i].item == entity)
        {
            int end = render_object_count - 1;
            for (int j = i; j < end; ++j)
                render_objects[j] = render_objects[j+1];
            --render_object_count;
            return;
        }
    }

    LOG_ERR("entity_free: could not find entity in render list");
}

projectile_s* projectile_alloc(void)
{
    for (int i = 0; i < MAX_PROJECTILE_COUNT; ++i)
    {
        projectile_s *proj = g_game.projectiles + i;
        if (IS_PROJ_ACTIVE(proj)) continue;

        *proj = (projectile_s)
        {
            .flags = PROJ_FLAG_ACTIVE,
        };

        if (render_object_count == MAX_RENDER_OBJS)
            LOG_ERR("render object pool is full!");
        else
            render_objects[render_object_count++] = (render_obj_s)
            {
                .type = RENDER_OBJ_PROJECTILE,
                .item = proj
            };
        
        game_physics_on_proj_alloc(proj);
        return proj;
    }

    LOG_ERR("projectile pool is full!");
    return NULL;
}

void projectile_free(projectile_s *proj)
{
    if (!IS_PROJ_ACTIVE(proj)) return;

    game_physics_on_proj_free(proj);
    proj->flags = 0;

    // remove from render list
    for (int i = 0; i < render_object_count; ++i)
    {
        if (render_objects[i].item == proj)
        {
            int end = render_object_count - 1;
            for (int j = i; j < end; ++j)
                render_objects[j] = render_objects[j+1];
            --render_object_count;
            return;
        }
    }

    LOG_ERR("projectile_free: could not find projectile in render list");
}

bool projectile_queue_free(projectile_s *proj)
{
    if (proj_free_queue_count >= PROJ_FREE_QUEUE_MAX_SIZE)
        return false;

    proj_free_queue[proj_free_queue_count++] = proj;
    proj->flags |= PROJ_FLAG_QFREE;
    return true;
}

static void update_entities(void)
{
    const FIXED terminal_vel = int2fx(5);

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *entity = g_game.entities + i;
        if (!(entity->flags & ENTITY_FLAG_ENABLED)) continue;

        if (entity->behavior && entity->behavior->update)
        {
            entity->behavior->update(entity);
        }

        if (entity->flags & ENTITY_FLAG_ACTOR)
        {
            const int actor_flags = (int) entity->actor.flags;
            int move_x = (int) entity->actor.move_x;

            if (move_x != 0)
            {
                entity->actor.face_dir = (s8) sgn(move_x);
                entity->vel.x += fxmul(entity->actor.move_accel,
                                       int2fx(move_x));
                
                if (ABS(entity->vel.x) > entity->actor.move_speed)
                {
                    entity->vel.x = fxmul(entity->actor.move_speed,
                                          int2fx(SGN(entity->vel.x)));
                }
            }
            else
            {
                int sign = SGN3(entity->vel.x);
                entity->vel.x += fxmul(entity->actor.move_accel,
                                       int2fx(-sign));
                
                if (SGN3(entity->vel.x) != sign)
                    entity->vel.x = 0;
            }

            uint jump_trigger = (uint) entity->actor.jump_trigger;
            if (jump_trigger > 0)
            {
                if (actor_flags & ACTOR_FLAG_GROUNDED &&
                    actor_flags & ACTOR_FLAG_CAN_MOVE)
                {
                    entity->vel.y = -entity->actor.jump_velocity;
                    jump_trigger = 0;
                }
                else
                {
                    --jump_trigger;
                }
            }

            entity->actor.jump_trigger = (u8) jump_trigger;
        }

        if (entity->flags & ENTITY_FLAG_MOVING)
        {
            FIXED g = fxmul(GRAVITY, entity->gmult);
            entity->vel.y += g;

            if (entity->vel.y > terminal_vel)
                entity->vel.y = terminal_vel;

            // if this entity can collide, movement is done in update-physics
            if (!(entity->flags & ENTITY_FLAG_COLLIDE))
            {
                entity->pos.x += entity->vel.x;
                entity->pos.y += entity->vel.y;
            }
        }
    }
}

static void update_projectiles()
{
    for (int i = 0; i < MAX_PROJECTILE_COUNT; ++i)
    {
        projectile_s *proj = g_game.projectiles + i;
        if (!IS_PROJ_ACTIVE(proj)) continue;

        if (--proj->life == 0)
            projectile_free(proj);
    }
}

void game_init(void)
{
    g_game.cam_x = 0;
    g_game.cam_y = 0;
    g_game.input_enabled = true;
    
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        g_game.entities[i].flags = 0;
    }

    g_game.room_trans = (room_trans_state_s)
    {
        .phase = 0
    };

    game_physics_init();
}

void game_update(void)
{
    for (int i = 0; i < proj_free_queue_count; ++i)
        projectile_free(proj_free_queue[i]);
    proj_free_queue_count = 0;

    update_entities();
    update_projectiles();
    game_physics_update();
}

#define READ8(ptr, accum) (accum = *((ptr)++), \
                           accum)
#define READ16(ptr, accum) (accum = *((ptr)++), \
                           accum |= *((ptr)++) << 8, \
                           accum)
#define READ32(ptr, accum) (accum = *((ptr)++), \
                           accum |= *((ptr)++) << 8, \
                           accum |= *((ptr)++) << 16, \
                           accum |= *((ptr)++) << 24, \
                           accum)

void game_load_room(const map_header_s *map)
{
    g_game.map = map;
    g_game.room_collision = map_collision_data(map);
    g_game.room_width = (int) map->width;
    g_game.room_height = (int) map->height;

    u32 accum;

    const u8 *data_ptr = map_entity_data(map);
    int ent_count = READ16(data_ptr, accum);

    for (int i = 0; i < ent_count; ++i)
    {
        int chunk_size = READ16(data_ptr, accum);
        const u8 *chunk_ptr = data_ptr;
        data_ptr += chunk_size;

        s16 ex = READ16(chunk_ptr, accum);
        s16 ey = READ16(chunk_ptr, accum);
        s16 ew = READ16(chunk_ptr, accum);
        s16 eh = READ16(chunk_ptr, accum);
        const char *name = (const char *)chunk_ptr;
        while (*(chunk_ptr++) != 0);

        int prop_count = READ8(chunk_ptr, accum);

        // oh. Bruh. i wrote this entire malloc implementation only to then
        // realize "wait. actually there won't be any more than one property per
        // object. so like. i can just allocate this on the stack."
        entity_load_prop_s props[8];

        if (prop_count > 8)
        {
            LOG_ERR("only up to 8 entity properties supported!");
            continue;
        }

        for (int i = 0; i < prop_count; ++i)
        {
            const char *prop_name = (const char *)chunk_ptr;
            while (*(chunk_ptr++) != 0);
            entity_load_prop_type_e prop_type = READ8(chunk_ptr, accum);

            props[i].name = prop_name;
            props[i].type = prop_type;

            switch (prop_type)
            {
            case ELPT_STRING:
            {
                const char *prop_data_str = (const char *)chunk_ptr;
                while (*(chunk_ptr++) != 0);

                props[i].data = (uintptr_t) prop_data_str;
                break;
            }

            case ELPT_INT:
            {
                int prop_data_int = READ32(chunk_ptr, accum);
                props[i].data = (uintptr_t) prop_data_int;
                break;
            }

            case ELPT_DECIMAL: // fixed-point
            {
                FIXED prop_data_fx = READ32(chunk_ptr, accum);
                props[i].data = (uintptr_t) prop_data_fx;
                break;
            }

            default:
                LOG_ERR("unknown property data type %i", (int) prop_type);
                break;
            }
        }

        entity_load_s load_ent = (entity_load_s)
        {
            .x = int2fx((int) ex),
            .y = int2fx((int) ey),
            .w = int2fx((int) ew),
            .h = int2fx((int) eh),
            .name = name,
            .prop_count = prop_count,
            .props = props,
        };

        game_load_entity(&load_ent);
    }
}

static inline int render_obj_zidx(const render_obj_s *obj)
{
    if (obj->type == RENDER_OBJ_SPRITE)
        return ((entity_s *)obj->item)->sprite.zidx;
    else
        return 0;
}

void game_render(int *p_last_obj_index)
{
    // sort render list by z index
    for (int i = 1; i < render_object_count; ++i)
    {
        for (int j = i; j > 0; --j)
        {
            if (render_obj_zidx(&render_objects[j]) > render_obj_zidx(&render_objects[j-1]))
            {
                render_obj_s temp = render_objects[j];
                render_objects[j] = render_objects[j-1];
                render_objects[j-1] = temp;
            }
        }
    }

    gfx_scroll_x = g_game.cam_x * 2;
    gfx_scroll_y = g_game.cam_y * 2;

    const gfx_root_header_s *gfx_root_header =
        (const gfx_root_header_s *)game_sprdb_data;
    
    const gfx_sprite_s *gfx_sprites =
        (const gfx_sprite_s *)&gfx_root_header->sprite0;
    const gfx_frame_s *frame_pool =
        (const gfx_frame_s *)((uintptr_t)gfx_root_header + gfx_root_header->frame_pool);
    const gfx_obj_s *obj_pool =
        (const gfx_obj_s *)((uintptr_t)gfx_root_header + gfx_root_header->obj_pool);

    int obj_index = 0;
    for (int i = 0; i < render_object_count; ++i)
    {
        const render_obj_s *robj = render_objects + i;

        int sprite_graphic_id;
        int sprite_frame;
        bool sprite_hflip, sprite_vflip;
        int draw_x, draw_y;

        if (robj->type == RENDER_OBJ_SPRITE)
        {
            entity_s *ent = (entity_s *)robj->item;

            draw_x = (ent->pos.x >> FIX_SHIFT) + ent->sprite.ox;
            draw_y = (ent->pos.y >> FIX_SHIFT) + ent->sprite.oy;
            sprite_graphic_id = (int) ent->sprite.graphic_id;
            sprite_frame = (int) ent->sprite.frame;
            sprite_hflip = ent->sprite.flags & SPRITE_FLAG_FLIP_X;
            sprite_vflip = ent->sprite.flags & SPRITE_FLAG_FLIP_Y;
        }
        else if (robj->type == RENDER_OBJ_PROJECTILE)
        {
            projectile_s *proj = (projectile_s *)robj->item;
            draw_x = (proj->px >> FIX_SHIFT) - 4;
            draw_y = (proj->py >> FIX_SHIFT) - 4;
            sprite_graphic_id = (int) proj->graphic_id;
            sprite_frame = 0;
            sprite_hflip = false;
            sprite_vflip = false;
        }
        else
        {
            LOG_ERR("unknown render object type %i", (int) robj->type);
            continue;
        }

        int draw_cam_x = (draw_x - g_game.cam_x) * 2;
        int draw_cam_y = (draw_y - g_game.cam_y) * 2;

        // frustum culling. needed so sprites don't wrap around the screen.
        // and also obviously the performance benefit.
        if (draw_cam_x < -32 ||
            draw_cam_y < -32 ||
            draw_cam_x > SCREEN_WIDTH + 32 ||
            draw_cam_y > SCREEN_HEIGHT + 32)
        {
            continue;
        }

        const gfx_sprite_s *spr = &gfx_sprites[sprite_graphic_id];
        const gfx_frame_s *frame = frame_pool + spr->frame_pool_idx + sprite_frame;
        const gfx_obj_s *objs = obj_pool + frame->obj_pool_index;
        
        int frame_obj_count = frame->obj_count;

        // draw object assembly
        for (int j = 0; j < frame_obj_count; ++j)
        {
            const gfx_obj_s *obj_src = &objs[j];
            OBJ_ATTR *obj_dst = &gfx_oam_buffer[obj_index];

            int ox, oy;

            int flip_flags = 0;
            if (sprite_hflip)
            {
                flip_flags |= ATTR1_HFLIP;
                ox = obj_src->flipped_ox;
            }
            else
            {
                ox = obj_src->ox;
            }

            if (sprite_vflip)
            {
                flip_flags |= ATTR1_VFLIP;
                oy = obj_src->flipped_oy;
            }
            else
            {
                oy = obj_src->oy;
            }

            obj_set_attr(obj_dst, obj_src->a0, obj_src->a1 | (u16)flip_flags,
                         obj_src->a2 | ATTR2_PALBANK(0));
            obj_set_pos(obj_dst, draw_cam_x + ox, draw_cam_y + oy);
            
            if (++obj_index >= 64) goto exit_entity_loop;
        }
    }
    exit_entity_loop:;

    // update sprite animation
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *ent = g_game.entities + i;
        if (!(ent->flags & ENTITY_FLAG_ENABLED)) continue;
        if (!(ent->sprite.flags & SPRITE_FLAG_PLAYING)) continue;

        int sprite_time_accum = ent->sprite.accum;
        int sprite_frame = ent->sprite.frame;

        const gfx_sprite_s *spr = &gfx_sprites[ent->sprite.graphic_id];
        const gfx_frame_s *frame = frame_pool + spr->frame_pool_idx + sprite_frame;

        int frame_count = spr->frame_count;
        int frame_len = frame->frame_len;

        if (++sprite_time_accum >= frame_len)
        {
            sprite_time_accum = 0;
            if (sprite_frame == frame_count - 1)
            {
                if (spr->loop)
                    sprite_frame = 0;
                else
                    ent->sprite.flags &= ~SPRITE_FLAG_PLAYING;
            }
            else
            {
                ++sprite_frame;
            }
        }

        ent->sprite.frame = sprite_frame;
        ent->sprite.accum = sprite_time_accum;
    }

    int last_obj_index = *p_last_obj_index;
    for (int i = obj_index; i < last_obj_index; ++i)
    {
        obj_hide(&gfx_oam_buffer[i]);
    }

    *p_last_obj_index = obj_index;
}

static void change_room(const map_header_s *new_room)
{
    // remove all entities in the world, except ones with the
    // keep-on-room-change flag (i.e. the player and the platform cursor)
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *ent = &g_game.entities[i];

        if (!(ent->flags & ENTITY_FLAG_ENABLED)) continue;
        if (ent->flags & ENTITY_FLAG_KEEP_ON_ROOM_CHANGE) continue;

        entity_free(ent);
    }

    // remove all projectiles
    for (int i = 0; i < MAX_PROJECTILE_COUNT; ++i)
    {
        projectile_s *proj = &g_game.projectiles[i];
        if (!IS_PROJ_ACTIVE(proj)) continue;
        projectile_free(proj);
    }

    game_load_room(new_room);
    gfx_load_map(new_room);
}

static void room_transition_inactive_update(entity_s *player)
{
    FIXED roomw = int2fx(g_game.room_width * WORLD_TILE_SIZE);
    FIXED roomh = int2fx(g_game.room_height * WORLD_TILE_SIZE);
    FIXED colw = int2fx((int) player->col.w);
    FIXED colh = int2fx((int) player->col.h);
    FIXED cx = player->pos.x + colw / 2;
    FIXED cy = player->pos.y + colh / 2;

    int matx, maty;
    dir4_e dir;
    int player_mx = 0;

    if (cx > roomw)
    {
        matx = (int) g_game.map->px +
               g_game.map->width / WORLD_MATRIX_GRID_WIDTH;
        maty = (int) g_game.map->py +
               fx2int(cy) / (WORLD_MATRIX_GRID_HEIGHT * WORLD_TILE_SIZE);

        dir = DIR4_RIGHT;
        player_mx = 1;
        cx = roomw;
    }
    else if (cx < 0)
    {
        matx = (int) g_game.map->px - 1;
        maty = (int) g_game.map->py +
               fx2int(cy) / (WORLD_MATRIX_GRID_HEIGHT * WORLD_TILE_SIZE);

        dir = DIR4_LEFT;
        player_mx = -1;
        cx = 0;
    }
    else if (cy > roomh)
    {
        matx = (int) g_game.map->px +
               fx2int(cx) / (WORLD_MATRIX_GRID_WIDTH * WORLD_TILE_SIZE);
        maty = (int) g_game.map->py +
               g_game.map->height / WORLD_MATRIX_GRID_HEIGHT;
        
        dir = DIR4_DOWN;
        player_mx = 0;
        cy = roomh;
    }
    else if (cy < 0)
    {
        matx = (int) g_game.map->px +
               fx2int(cx) / (WORLD_MATRIX_GRID_WIDTH * WORLD_TILE_SIZE);
        maty = (int) g_game.map->py - 1;
        
        dir = DIR4_UP;
        player_mx = 0;
        cy = 0;
    }
    else return;

    const map_header_s *new_room;

    uint new_room_idx = (uint) world_matrix[maty][matx];
    if (new_room_idx == 0)
    {
        LOG_ERR("screen (%i, %i) is unassigned!!", matx, maty);
        return;
    }
    --new_room_idx;

    new_room = world_rooms[new_room_idx];

    g_game.room_trans = (room_trans_state_s)
    {
        .phase = 1,
        .ticks = 0,
        .dir = dir,
        .new_room = new_room,
        .override_player_move_x = true,
        .player_move_x = player_mx
    };
}

static void room_transition_phase1_update(entity_s *player)
{
    if (g_game.room_trans.dir == DIR4_UP)
        player->vel.y = TO_FIXED(-1);

    FIXED fac = g_game.room_trans.ticks * (FIX_ONE / 20);
    gfx_set_palette_multiplied(FIX_ONE - fac);
    if (++g_game.room_trans.ticks < 30) return;

    const map_header_s *old_room = g_game.map;
    const map_header_s *new_room = g_game.room_trans.new_room;
    change_room(new_room);

    FIXED colw = int2fx((int) player->col.w);
    FIXED colh = int2fx((int) player->col.h);
    FIXED cx = player->pos.x + colw / 2;
    FIXED cy = player->pos.y + colh / 2;

    int player_mx = 0;

    switch (g_game.room_trans.dir)
    {
    case DIR4_RIGHT:
        cx = 0;
        cy += int2fx((old_room->py - new_room->py) * WORLD_MATRIX_GRID_HEIGHT * WORLD_TILE_SIZE);
        player->vel.y = 0;
        player_mx = 1;
        break;

    case DIR4_LEFT:
        cx = int2fx(new_room->width * WORLD_TILE_SIZE);
        cy += int2fx((old_room->py - new_room->py) * WORLD_MATRIX_GRID_HEIGHT * WORLD_TILE_SIZE);
        player->vel.y = 0;
        player_mx = -1;
        break;

    case DIR4_DOWN:
        cx += int2fx((old_room->px - new_room->px) * WORLD_MATRIX_GRID_WIDTH * WORLD_TILE_SIZE);
        cy = 0;
        player->vel.x = 0;
        player->vel.y = TO_FIXED(1);
        player_mx = 0;
        break;

    case DIR4_UP:
        cx += int2fx((old_room->px - new_room->px) * WORLD_MATRIX_GRID_WIDTH * WORLD_TILE_SIZE);
        cy = int2fx(new_room->height * WORLD_TILE_SIZE);
        player->vel.x = 0;
        player->vel.y = TO_FIXED(-2);
        player_mx = player->actor.face_dir;
        break;

    default: LOG_ERR("unreachable switch statement wtf???");
    }

    player->pos.x = cx - colw / 2;
    player->pos.y = cy - colh / 2;

    g_game.room_trans = (room_trans_state_s)
    {
        .phase = 2,
        .ticks = 0,
        .player_move_x = player_mx,
        .override_player_move_x = true,
    };
}

static void room_transition_phase2_update(entity_s *player)
{
    FIXED fac = g_game.room_trans.ticks * (FIX_ONE / 20);
    gfx_set_palette_multiplied(fac);

    ++g_game.room_trans.ticks;

    if (g_game.room_trans.ticks == 15)
        g_game.room_trans.override_player_move_x = false;

    if (g_game.room_trans.ticks == 30)
    {
        g_game.room_trans.phase = 0;
        gfx_reset_palette();
    }
}

void game_transition_update(entity_s *player)
{
    switch (g_game.room_trans.phase)
    {
    case 0:
        room_transition_inactive_update(player);
        break;

    case 1:
        room_transition_phase1_update(player);
        break;

    case 2:
        room_transition_phase2_update(player);
        break;
    }
}