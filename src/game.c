#include <stdlib.h>
#include <tonc.h>
#include <world.h>
#include <string.h>

#include "game.h"
#include "game_physics.h"
#include "log.h"
#include "gfx.h"
#include "math_util.h"
#include "gba_util.h"

#define MAX_RENDER_OBJS ((MAX_ENTITY_COUNT + MAX_PROJECTILE_COUNT))
#define FREE_QUEUE_MAX_SIZE 32

// 1 KiB for the copy of the map collision. stored in ewram.
#define GAME_COLLISION_MAP_SIZE (1024)

#define GAME_BG_IDX 1

typedef enum render_obj_t
{
    RENDER_OBJ_SPRITE,
    RENDER_OBJ_PROJECTILE
} render_obj_t_e;

typedef struct render_obj
{
    u8 type;
    void *item;
    s16 zidx;
}
render_obj_s;

typedef struct game_state
{
    entity_s entities[MAX_ENTITY_COUNT];
    projectile_s projectiles[MAX_PROJECTILE_COUNT];
    int cam_x, cam_y;
    room_trans_state_s room_trans;
    entity_s *active_water_tank;
    uint player_ammo;
    uint player_spit_mode;
    bool did_collect_orb;
    u8 collected_rorbs;
    u8 collected_borbs;
}
game_state_s;

game_s g_game;
EWRAM_BSS game_state_s game_saved_state;
EWRAM_BSS static u8 game_room_collision[GAME_COLLISION_MAP_SIZE];

static uint render_object_count = 0;
static render_obj_s render_objects[MAX_RENDER_OBJS];

static int ent_free_queue_count = 0;
static entity_s *ent_free_queue[FREE_QUEUE_MAX_SIZE];

static int proj_free_queue_count = 0;
static projectile_s *proj_free_queue[FREE_QUEUE_MAX_SIZE];

static int last_obj_index = 0;

EWRAM_BSS char game_dialogue_buffer[DIALOGUE_BUFFER_SIZE];

static bool game_transition_update(entity_s *player);

// this is called by both entity_alloc and game_restore_state
static void on_entity_alloc(entity_s *ent)
{
    if (render_object_count == MAX_RENDER_OBJS)
        LOG_ERR("render object pool is full!");
    else
        render_objects[render_object_count++] = (render_obj_s)
        {
            .type = RENDER_OBJ_SPRITE,
            .item = ent
        };
    
    game_physics_on_entity_alloc(ent);
}

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

        on_entity_alloc(ent);
        return ent;
    }

    LOG_ERR("entity pool is full!");
    return NULL;
}

void entity_free(entity_s *entity)
{
    if (!entity) return;
    // given entity is already free, don't do anything
    if (!(entity->flags & ENTITY_FLAG_ENABLED)) return;

    game_physics_on_entity_free(entity);

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

bool entity_queue_free(entity_s *ent)
{
    if (ent->flags & ENTITY_FLAG_QFREE) return true;
    if (ent_free_queue_count >= FREE_QUEUE_MAX_SIZE)
        return false;

    ent_free_queue[ent_free_queue_count++] = ent;
    ent->flags |= ENTITY_FLAG_QFREE;
    return true;
}

// this is called by both projectile_alloc and game_restore_state
static void on_projectile_alloc(projectile_s *proj)
{
    if (render_object_count == MAX_RENDER_OBJS)
        LOG_ERR("render object pool is full!");
    else
        render_objects[render_object_count++] = (render_obj_s)
        {
            .type = RENDER_OBJ_PROJECTILE,
            .item = proj
        };
    
    game_physics_on_proj_alloc(proj);
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

        on_projectile_alloc(proj);
        return proj;
    }

    LOG_ERR("projectile pool is full!");
    return NULL;
}

void projectile_free(projectile_s *proj)
{
    if (!proj) return;
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
    if (proj->flags & PROJ_FLAG_QFREE) return true;
    if (proj_free_queue_count >= FREE_QUEUE_MAX_SIZE)
    {
        LOG_WRN("free queue is full!");
        return false;
    }

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
            entity->actor.flags &= ~ACTOR_FLAG_DID_JUMP;
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
                    entity->actor.flags |= ACTOR_FLAG_DID_JUMP;
                }
                else
                {
                    --jump_trigger;
                }
            }

            entity->actor.jump_trigger = (u8) jump_trigger;
        }

        if (entity->flags & ENTITY_FLAG_DAMPING)
        {
            FIXED vx = entity->vel.x;
            entity->vel.x = fxmul(vx, entity->damp);

            // integer rounding is towards negative infinity, so add +1 when
            // negative to make it round to zero. otherwise it will approach
            // some negative number rather than zero.
            if (entity->vel.x < 0) ++entity->vel.x;
        }

        if (entity->flags & ENTITY_FLAG_MOVING)
        {
            FIXED g = fxmul(WORLD_GRAVITY, entity->gmult);
            entity->vel.y += g;

            // water buoyancy
            if (entity->flags & ENTITY_FLAG_COLLIDE)
            {
                int cflags = entity->col.flags;
                cflags &= ~COL_FLAG_IN_WATER;

                FIXED cx = entity->pos.x + int2fx(entity->col.w) / 2;
                FIXED cy = entity->pos.y + int2fx(entity->col.h) / 2;
                int tx = cx / (WORLD_TILE_SIZE * FIX_ONE);
                int ty = cy / (WORLD_TILE_SIZE * FIX_ONE);

                if (game_get_col_clamped(tx, ty) == 2)
                {
                    cflags |= COL_FLAG_IN_WATER;
                    int ty2 = (cy - int2fx(1)) / (WORLD_TILE_SIZE * FIX_ONE);

                    if (game_get_col_clamped(tx, ty2) != 2 &&
                        abs(entity->vel.y) < TO_FIXED(0.125))
                    {
                        entity->vel.y = 0;
                    }
                    else
                    {
                        entity->vel.y = fxmul(entity->vel.y, TO_FIXED(0.8));
                        entity->vel.y -= g + TO_FIXED(0.09375) / entity->mass;
                    }
                }

                entity->col.flags = cflags;
            }

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

static void update_animation()
{
    const gfx_root_header_s *gfx_root_header =
        (const gfx_root_header_s *)game_sprdb_bin;
    
    const gfx_sprite_s *gfx_sprites =
        (const gfx_sprite_s *)&gfx_root_header->sprite0;
    const gfx_frame_s *frame_pool =
        (const gfx_frame_s *)((uintptr_t)gfx_root_header + gfx_root_header->frame_pool);
    
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

    // timer * 4.25
    // offset by 0.5 for better pixel rounding
    uint timer = g_game.active_interactable_timer;
    g_game.interactable_indicator_offset =
        sine_lut(timer * 4 + timer / 4) + TO_FIXED(0.5);
}

void dialogue_show_page(void)
{
    if (g_game.dialogue_page == NULL || *g_game.dialogue_page == 0)
    {
        g_game.dialogue_active = false;
        return;
    }
    
    int line_count = 1;
    for (const char *c = g_game.dialogue_page; *c != '\f'; ++c)
        if (*c == '\n') ++line_count;
    
    const int row_count = ceil_div(line_count * 12, 8);
    u32 full_bitmap[] = {0x11111111, 0x11111111, 0x11111111, 0x11111111,
                         0x11111111, 0x11111111, 0x11111111, 0x11111111};

    int bitmap_row_overflow = (2 + (line_count - 1) * 4) % 8;
    u32 partial_bitmap[8] = {0x00000000, 0x00000000, 0x00000000, 0x00000000,
                             0x00000000, 0x00000000, 0x00000000, 0x00000000};
    for (int i = 0; i < bitmap_row_overflow; ++i)
    {
        partial_bitmap[i] = 0x11111111;
    }

    gfx_text_bmap_fill(0, 0, GFX_TEXT_BMP_COLS, row_count - 1, full_bitmap);
    gfx_text_bmap_fill(0, row_count - 1, GFX_TEXT_BMP_COLS, 1, partial_bitmap);

    char line_buf[21];
    uint lb_i = 0;
    int ypos = 0;
    const char *ch = g_game.dialogue_page;
    for (; *ch != '\f'; ++ch)
    {
        if (*ch == '\n')
        {
            line_buf[lb_i] = '\0';
            LOG_DBG("Line: %s\n", line_buf);
            gfx_text_bmap_print(0, ypos, line_buf, TEXT_COLOR_WHITE);
            ypos += 12;
            line_buf[0] = '\0';
            lb_i = 0;
            continue;
        }

        line_buf[lb_i++] = *ch;
    }

    line_buf[lb_i] = '\0';
    gfx_text_bmap_print(0, ypos, line_buf, TEXT_COLOR_WHITE);

    g_game.dialogue_active = true;
    g_game.dialogue_page = ch + 1;
}

static void update_dialogue()
{
    if (key_hit(KEY_A | KEY_B))
    {
        u32 bitmap[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000,
                        0x00000000, 0x00000000, 0x00000000, 0x00000000};
        gfx_text_bmap_fill(0, 0, GFX_TEXT_BMP_COLS, 9, bitmap);

        dialogue_show_page();
    }
}

static void update_camera(entity_s *player)
{
    const FIXED dx_max = FX(10);
    FIXED dx = player->pos.x - g_game.cam_x;
    FIXED dy = player->pos.y - g_game.cam_y;

    game_camera_s *const cam_data = &g_game.cam_data;

    // if (dx > dx_max)
    // {
    //     cam_data->move_x = 1;
    // }
    // else if (dx < -dx_max)
    // {
    //     cam_data->move_x = -1;
    // }

    // if (cam_data->move_x != 0)
    // {
    //     cam_data->target_x = player->pos.x + FX(8) * cam_data->move_x;
    // }

    // if (abs(dy) > FX(20)) cam_data->y_follow = true;

    // bool grounded = player->actor.flags & ACTOR_FLAG_GROUNDED;
    // if (grounded) cam_data->y_follow = false;

    // if (grounded || cam_data->y_follow)
    //     cam_data->target_y = player->pos.y;

    FIXED target_x = player->actor.face_dir * FX(12);

    // cam_data->vx += fxmul(target_x - cam_data->rel_x, FX(0.01))
    //                 - fxmul(cam_data->vx, FX(0.15));
    {
        int sgn = sgn3(target_x - cam_data->rel_x);
        cam_data->rel_x += fxmul(FX(sgn), FX(0.5));
        if (sgn3(target_x - cam_data->rel_x) != sgn)
            cam_data->rel_x = target_x;
    }

    // cam_data->rel_x += cam_data->vx;
    g_game.cam_x = player->pos.x + cam_data->rel_x;

    // cam_data->vy += fxmul(cam_data->target_y - g_game.cam_y, FX(0.01))
    //                 - fxmul(cam_data->vy, FX(0.15));
    // g_game.cam_y += cam_data->vy;

    const FIXED dy_max = FX(12);

    if (dy > dy_max)
    {
        g_game.cam_y = player->pos.y - dy_max;
        // cam_data->move_y = 1;
    }

    if (dy < -dy_max)
    {
        g_game.cam_y = player->pos.y + dy_max;
        // cam_data->move_y = -1;
    }

    // if (cam_data->move_y != 0)
    // {
    //     g_game.cam_y += FX(1) * cam_data->move_y;
    //     FIXED new_dy = player->pos.y - g_game.cam_y;
    //     if (new_dy > dy_max)
    //         g_game.cam_y = player->pos.y - dy_max;
    //     else if (new_dy < -dy_max)
    //         g_game.cam_y = player->pos.y + dy_max;

    //     if (sgn3(dy) != sgn3(new_dy))
    //     {
    //         g_game.cam_y = player->pos.y;
    //         cam_data->move_y = 0;
    //     }   
    // }

    // if (abs(cam_data->vx) < FX(0.2))
    // {
    //     cam_data->move_x = 0;
    // }
}

void game_init(void)
{
    g_game = (game_s)
    {
        .input_enabled = true,
        .player_ammo = 100,
        .room_trans = (room_trans_state_s)
        {
            .phase = 0
        },
        .cam_x = int2fx(SCREEN_WIDTH / 4),
        .cam_y = int2fx(SCREEN_HEIGHT / 4),
    };

    last_obj_index = 0;
    ent_free_queue_count = 0;
    proj_free_queue_count = 0;
    render_object_count = 0;

    game_physics_init();

    entity_s *player = entity_alloc();
    entity_player_init(player);
    player->pos.x = int2fx(16);
    player->pos.y = int2fx(16);
}

void game_update(void)
{
    if (g_game.queue_dialogue_start)
    {
        g_game.queue_dialogue_start = false;
        dialogue_show_page();
    }

    if (g_game.dialogue_active)
    {
        update_dialogue();
        return;
    }

    entity_s *player = &g_game.entities[0];
    g_game.active_interactable = NULL;

    if (!game_transition_update(player)) return;

    update_entities();
    update_projectiles();
    game_physics_update();
    update_animation();

    if (g_game.queue_restore)
    {
        g_game.queue_restore = false;
        game_restore_state();
    }

    update_camera(player);

    const FIXED x_min = int2fx(SCREEN_WIDTH / 4);
    const FIXED y_min = int2fx(SCREEN_HEIGHT / 4);
    const FIXED x_max = int2fx(gfx_ctl.bg[GAME_BG_IDX].map_width * 8
                               - SCREEN_WIDTH / 4);
    const FIXED y_max = int2fx(gfx_ctl.bg[GAME_BG_IDX].map_height * 8
                               - SCREEN_HEIGHT / 4);

    if (g_game.cam_x < x_min) g_game.cam_x = x_min;
    if (g_game.cam_y < y_min) g_game.cam_y = y_min;
    if (g_game.cam_x > x_max) g_game.cam_x = x_max;
    if (g_game.cam_y > y_max) g_game.cam_y = y_max;

    for (int i = 0; i < proj_free_queue_count; ++i)
        projectile_free(proj_free_queue[i]);
    proj_free_queue_count = 0;

    for (int i = 0; i < ent_free_queue_count; ++i)
        entity_free(ent_free_queue[i]);
    ent_free_queue_count = 0;
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
    g_game.room_collision = game_room_collision;
    g_game.room_width = (int) map->width;
    g_game.room_height = (int) map->height;

    const u8 *col_data = map_collision_data(map);
    const int col_data_size = map_collision_data_size(map);
    if (CEIL_DIV(col_data_size, 4) > GAME_COLLISION_MAP_SIZE)
    {
        LOG_ERR("map collision too large to copy to iwram!");
        DBG_CRASH();
    }

    memcpy32(game_room_collision, col_data, CEIL_DIV(col_data_size, 4));

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

ARM_FUNC NO_INLINE
static void sort_render_list(void)
{
    // precalculate object z-indices
    for (int i = 1; i < render_object_count; ++i)
    {
        render_obj_s *const obj = render_objects + i;
        obj->zidx = render_obj_zidx(obj);
    }

    // sort render list by z index
    for (int i = 1; i < render_object_count; ++i)
    {
        for (int j = i; j > 0; --j)
        {
            if (render_objects[j].zidx > render_objects[j-1].zidx)
            {
                render_obj_s temp = render_objects[j];
                render_objects[j] = render_objects[j-1];
                render_objects[j-1] = temp;
            }
        }
    }
}

static inline bool renderer_cam_calc(int obj_x, int obj_y, int cam_x,
                                     int cam_y, int *draw_x,
                                     int *draw_y)
{
    *draw_x = (obj_x - cam_x) * 2 + SCREEN_WIDTH / 2;
    *draw_y = (obj_y - cam_y) * 2 + SCREEN_HEIGHT / 2;

    // frustum culling. needed so sprites don't wrap around the screen.
    // and also obviously the performance benefit.
    return (*draw_x < -32 ||
            *draw_y < -32 ||
            *draw_x > SCREEN_WIDTH + 32 ||
            *draw_y > SCREEN_HEIGHT + 32);
}

void game_render(void)
{
    const int cam_x = fx2int(g_game.cam_x);
    const int cam_y = fx2int(g_game.cam_y);

    gfx_ctl.bg[GAME_BG_IDX].offset_x = cam_x * 2 - SCREEN_WIDTH / 2;
    gfx_ctl.bg[GAME_BG_IDX].offset_y = cam_y * 2 - SCREEN_HEIGHT / 2;

    gfx_sprdb_s sprdb =
        gfx_get_sprdb((const gfx_root_header_s *)game_sprdb_bin);

    OBJ_ATTR *const game_oam = gfx_oam_buffer + 64;

    gfx_draw_sprite_state_s draw_state = (gfx_draw_sprite_state_s)
    {
        .sprdb = &sprdb,
        .dst_obj = game_oam,
        .dst_obj_count = 64
    };

    // draw interactable indicator
    if (g_game.active_interactable)
    {
        const entity_s *const ent = g_game.active_interactable;
        FIXED dy = g_game.interactable_indicator_offset;
        
        // draw arrow
        draw_state.a0 = 0;
        draw_state.a1 = 0;
        draw_state.a2 = ATTR2_PALBANK(1) | ATTR2_PRIO(1);

        FIXED pos_x = ent->pos.x + int2fx(ent->col.w) / 2;
        FIXED pos_y = ent->pos.y - int2fx(5) + dy;

        int draw_cam_x, draw_cam_y;
        int draw_x = fx2int(pos_x) - 4;
        int draw_y = fx2int(pos_y) - 2;

        renderer_cam_calc(draw_x, draw_y, cam_x, cam_y, &draw_cam_x,
                          &draw_cam_y);
        gfx_draw_sprite(&draw_state, SPRID_GAME_DOWN_ARROW, 0,
                        draw_cam_x, draw_cam_y);

        // draw interact button
        draw_state.a0 = 0;
        draw_state.a1 = 0;
        draw_state.a2 = ATTR2_PALBANK(0) | ATTR2_PRIO(1);

        pos_y -= int2fx(6);
        draw_x = fx2int(pos_x) - 4;
        draw_y = fx2int(pos_y) - 2;

        renderer_cam_calc(draw_x, draw_y, cam_x, cam_y, &draw_cam_x,
                          &draw_cam_y);
        gfx_draw_sprite(&draw_state, SPRID_GAME_BUTTON_B, 0,
                        draw_cam_x, draw_cam_y);

        if (++g_game.active_interactable_timer == 61)
            g_game.active_interactable_timer = 0;
    }
    else
    {
        g_game.active_interactable_timer = 0;
    }
    

    sort_render_list();

    for (int i = 0; i < render_object_count; ++i)
    {
        const render_obj_s *robj = render_objects + i;

        int sprite_graphic_id;
        int sprite_frame;
        int sprite_palette;
        bool sprite_hflip, sprite_vflip;
        bool sprite_hidden = false;
        int draw_cam_x, draw_cam_y;

        if (robj->type == RENDER_OBJ_SPRITE)
        {
            entity_s *ent = (entity_s *)robj->item;
            
            int draw_x = fx2int(ent->pos.x) + ent->sprite.ox;
            int draw_y = fx2int(ent->pos.y) + ent->sprite.oy;

            if (renderer_cam_calc(draw_x, draw_y, cam_x, cam_y, &draw_cam_x,
                                  &draw_cam_y))
                continue;

            sprite_graphic_id = (int) ent->sprite.graphic_id;
            sprite_frame = (int) ent->sprite.frame;
            sprite_hflip = ent->sprite.flags & SPRITE_FLAG_FLIP_X;
            sprite_vflip = ent->sprite.flags & SPRITE_FLAG_FLIP_Y;
            sprite_palette = ent->sprite.palette;
            sprite_hidden = (ent->sprite.flags & SPRITE_FLAG_HIDDEN);
        }
        else if (robj->type == RENDER_OBJ_PROJECTILE)
        {
            projectile_s *proj = (projectile_s *)robj->item;

            int draw_x = fx2int(proj->px) - 4;
            int draw_y = fx2int(proj->py) - 4;

            if (renderer_cam_calc(draw_x, draw_y, cam_x, cam_y, &draw_cam_x,
                                  &draw_cam_y))
                continue;

            sprite_graphic_id = (int) proj->graphic_id;
            sprite_frame = 0;
            sprite_hflip = false;
            sprite_vflip = false;
            sprite_palette = 0;
        }
        else
        {
            LOG_ERR("unknown render object type %i", (int) robj->type);
            continue;
        }

        draw_state.a0 = 0;
        draw_state.a1 = 0;
        draw_state.a2 = 0;

        if (sprite_hidden)
            draw_state.a0 |= ATTR0_HIDE;
        if (sprite_hflip)
            draw_state.a1 |= ATTR1_HFLIP;
        if (sprite_vflip)
            draw_state.a1 |= ATTR1_VFLIP;

        draw_state.a2 |= ATTR2_PALBANK(sprite_palette) | ATTR2_PRIO(1);

        gfx_draw_sprite(&draw_state, sprite_graphic_id, sprite_frame,
                        draw_cam_x, draw_cam_y);
        if (draw_state.dst_obj_count == 0) break;
    }

    int obj_index = 64 - draw_state.dst_obj_count;
    int old_obj_count = last_obj_index;
    for (int i = obj_index; i < old_obj_count; ++i)
    {
        obj_hide(&game_oam[i]);
    }

    last_obj_index = obj_index;
}

static void change_room(const map_header_s *new_room)
{
    // if an orb was collected, commit the change
    if (g_game.did_collect_orb)
    {
        if (g_game.collected_orbs_count >= MAX_ORB_COUNT)
        {
            LOG_ERR("collected orb count list is full!");
            return;
        }
        g_game.collected_orbs[g_game.collected_orbs_count++] = g_game.map;
        g_game.did_collect_orb = false;

        g_game.committed_collected_rorbs = g_game.collected_rorbs;
        g_game.committed_collected_borbs = g_game.collected_borbs;
    }

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
    gfx_load_map(GAME_BG_IDX, new_room);
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

static bool room_transition_phase1_update(entity_s *player)
{
    if (g_game.room_trans.dir == DIR4_UP)
        player->vel.y = TO_FIXED(-1);

    if (++g_game.room_trans.ticks < 30)
    {
        FIXED fac = g_game.room_trans.ticks * (FIX_ONE / 20);
        gfx_set_palette_multiplied(FIX_ONE - fac);
        return true;
    }

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
    
    g_game.cam_x = player->pos.x;
    g_game.cam_y = player->pos.y;

    const FIXED x_min = int2fx(SCREEN_WIDTH / 4);
    const FIXED y_min = int2fx(SCREEN_HEIGHT / 4);
    const FIXED x_max = int2fx(gfx_ctl.bg[GAME_BG_IDX].map_width * 8
                               - SCREEN_WIDTH / 4);
    const FIXED y_max = int2fx(gfx_ctl.bg[GAME_BG_IDX].map_height * 8
                               - SCREEN_HEIGHT / 4);

    if (g_game.cam_x < x_min) g_game.cam_x = x_min;
    if (g_game.cam_y < y_min) g_game.cam_y = y_min;
    if (g_game.cam_x > x_max) g_game.cam_x = x_max;
    if (g_game.cam_y > y_max) g_game.cam_y = y_max;

    g_game.active_water_tank = NULL;

    game_save_state();

    return false;
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

static bool game_transition_update(entity_s *player)
{
    switch (g_game.room_trans.phase)
    {
    case 0:
        room_transition_inactive_update(player);
        return true;

    case 1:
        return room_transition_phase1_update(player);

    case 2:
        room_transition_phase2_update(player);
        return true;
    }

    return true;
}

void game_save_state(void)
{
    // copy entity data
    const entity_s *src_ent = g_game.entities;
    entity_s *dst_ent = game_saved_state.entities;

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        *dst_ent = *src_ent;
        ++dst_ent;
        ++src_ent;
    }

    // copy projectile data
    const projectile_s *src_proj = g_game.projectiles;
    projectile_s *dst_proj = game_saved_state.projectiles;

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        *dst_proj = *src_proj;
        ++dst_proj;
        ++src_proj;
    }

    game_saved_state.cam_x = g_game.cam_x;
    game_saved_state.cam_y = g_game.cam_y;
    game_saved_state.room_trans = g_game.room_trans;
    game_saved_state.active_water_tank = g_game.active_water_tank;
    game_saved_state.player_ammo = g_game.player_ammo;
    game_saved_state.player_spit_mode = g_game.player_spit_mode;
    game_saved_state.did_collect_orb = g_game.did_collect_orb;
    game_saved_state.collected_rorbs = g_game.collected_rorbs;
    game_saved_state.collected_borbs = g_game.collected_borbs;
}

void game_restore_state(void)
{
    // copy entity data, properly freeing newly unallocated slots
    const entity_s *src_ent = game_saved_state.entities;
    entity_s *dst_ent = g_game.entities;

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        if (!ENTITY_ENABLED(src_ent) && ENTITY_ENABLED(dst_ent))
        {
            entity_free(dst_ent);
        }
        else
        {
            bool need_alloc = !ENTITY_ENABLED(dst_ent) &&
                              ENTITY_ENABLED(src_ent);
            *dst_ent = *src_ent;
            if (need_alloc) on_entity_alloc(dst_ent);
        }

        ++dst_ent;
        ++src_ent;
    }

    // copy projectile data, properly freeing newly unallocated slots
    const projectile_s *src_proj = game_saved_state.projectiles;
    projectile_s *dst_proj = g_game.projectiles;

    for (int i = 0; i < MAX_PROJECTILE_COUNT; ++i)
    {
        if (!IS_PROJ_ACTIVE(src_proj) && IS_PROJ_ACTIVE(dst_proj))
        {
            projectile_free(dst_proj);
        }
        else
        {
            bool need_alloc = !IS_PROJ_ACTIVE(dst_proj) &&
                              IS_PROJ_ACTIVE(src_proj);
            *dst_proj = *src_proj;
            if (need_alloc) on_projectile_alloc(dst_proj);
        }

        ++dst_proj;
        ++src_proj;
    }

    g_game.cam_x = game_saved_state.cam_x;
    g_game.cam_y = game_saved_state.cam_y;
    g_game.room_trans = game_saved_state.room_trans;
    g_game.active_water_tank = game_saved_state.active_water_tank;
    g_game.player_ammo = game_saved_state.player_ammo;
    g_game.player_spit_mode = game_saved_state.player_spit_mode;
    g_game.did_collect_orb = game_saved_state.did_collect_orb;
    g_game.collected_rorbs = game_saved_state.collected_rorbs;
    g_game.collected_borbs = game_saved_state.collected_borbs;
    g_game.player_is_dead = false;

    gfx_mark_scroll_dirty(GAME_BG_IDX);
}

void game_start_dialogue(const char *dialogue)
{
    g_game.queue_dialogue_start = true;
    g_game.dialogue_page = dialogue;
}