#include <tonc.h>
#include <player_gfx.h>
#include <tileset_gfx.h>
#include <assert.h>
#include "mgba.h"

#include "map_data.h"
#include "gfx.h"

#define WORLD_SUBPX_SHIFT 4
#define WORLD_SUBPX_SCALE 16

#define ENTITY_FLAG_ENABLED  1
#define ENTITY_FLAG_MOVING   2
#define ENTITY_FLAG_ACTOR    4
#define ENTITY_FLAG_COLLIDE  8

#define ACTOR_FLAG_GROUNDED   1
#define ACTOR_FLAG_WALL       2
#define ACTOR_FLAG_DID_JUMP   4

#define COLGROUP_DEFAULT 1
#define COLGROUP_ALL UINT16_MAX

#define MAX_ENTITY_COUNT 32
#define MAX_PROJECTILE_COUNT 128
#define MAX_CONTACT_COUNT 64

// not the actual tile rendered tile size, which is 16. but tiles are supposed
// to be 8x8.
#define WORLD_TILE_SIZE 8

typedef enum entity_msgid
{
    ENTITY_MSG_INTERACT,
    ENTITY_MSG_ATTACKED
} entity_msgid_e;

typedef struct entity {
    u32 flags;

    struct { FIXED x; FIXED y; } pos;
    struct { FIXED x; FIXED y; } vel;
    u8 gmult; // 0 = no gravity, 255 = full gravity
    u8 mass; // 1 = 0.5 mass, 2 = 1 mass, 3 = 1.5 mass, e.t.c.
    u8 health;

    struct {
        u8 w; u8 h; // in world pixels
        u16 group; u16 mask;
    } col;

    struct {
        u8 flags;
        s8 move_x;
        s8 face_dir;
        u8 jump_trigger;

        FIXED move_speed;
        FIXED move_accel;
        FIXED jump_velocity;
    } actor;

    struct {
        u8 sprite_id;
        u8 anim_id;
        u8 frame;
        u8 accum;
        s8 ox;
        s8 oy;
    } sprite;

    s32 internal[4];
    void (*update)(struct entity *self);
    void (*message)(struct entity *self, entity_msgid_e msg, ...);
} entity_s;

typedef struct col_contact {
    FIXED px, py, pd;
    entity_s *ent_a, *ent_b;
} col_contact_s;

static_assert(sizeof(entity_s) % 4 == 0, "entity_s size must be word-aligned");
static entity_s entities[MAX_ENTITY_COUNT];

static uint col_contact_count = 0;
static col_contact_s col_contacts[MAX_CONTACT_COUNT];

static const u8 *map_collision;

static bool rect_collision(FIXED x0, FIXED y0, FIXED w0, FIXED h0,
                           FIXED x1, FIXED y1, FIXED w1, FIXED h1,
                           FIXED *nx, FIXED *ny, FIXED *pd)
{
    const FIXED l0 = x0;
    const FIXED t0 = y0;
    const FIXED r0 = x0 + w0;
    const FIXED b0 = y0 + h0;

    const FIXED l1 = x1;
    const FIXED t1 = y1;
    const FIXED r1 = x1 + w1;
    const FIXED b1 = y1 + h1;

    FIXED pl = r0 - l1;
    FIXED pt = b0 - t1;
    FIXED pr = r1 - l0;
    FIXED pb = b1 - t0;
    FIXED mp = min(pr, min(pt, min(pl, pb)));
    if (mp < 0)
        return false;

    if (mp == pl)
    {
        *nx = int2fx(-1);
        *ny = int2fx(0);
    }
    else if (mp == pt)
    {
        *nx = int2fx(0);
        *ny = int2fx(-1);
    }
    else if (mp == pr)
    {
        *nx = int2fx(1);
        *ny = int2fx(0);
    }
    else if (mp == pb)
    {
        *nx = int2fx(0);
        *ny = int2fx(1);
    }
    else return false;

    *pd = mp;
    return true;
}

void entity_init(entity_s *self)
{
    memset32(self, 0, sizeof(entity_s) / 4);
    self->gmult = 255;
    self->mass = 2;
    self->col.group = COLGROUP_DEFAULT;
    self->col.mask = COLGROUP_ALL;
}

void update_entities(void)
{
    const FIXED gravity = float2fx(0.1f);
    const FIXED terminal_vel = int2fx(5);

    col_contact_count = 0;

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *entity = entities + i;
        if (!(entity->flags & ENTITY_FLAG_ENABLED)) continue;

        if (entity->flags & ENTITY_FLAG_ACTOR)
        {
            if (entity->actor.move_x != 0)
            {
                entity->vel.x += fxmul(entity->actor.move_accel,
                                       int2fx((int) entity->actor.move_x));
                
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
                if (entity->actor.flags & ACTOR_FLAG_GROUNDED)
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
            FIXED g = fxmul(gravity, entity->gmult * (FIX_SCALE / 256));
            entity->vel.y += g;

            if (entity->vel.y > terminal_vel)
                entity->vel.y = terminal_vel;

            entity->pos.x += entity->vel.x;
            entity->pos.y += entity->vel.y;
        }

        if (entity->flags & ENTITY_FLAG_COLLIDE)
        {
            const FIXED col_w = int2fx((int)entity->col.w);
            const FIXED col_h = int2fx((int)entity->col.h);

            u32 actor_flags = entity->flags;
            actor_flags &= ~(ACTOR_FLAG_GROUNDED | ACTOR_FLAG_DID_JUMP);

            int min_x = entity->pos.x / (WORLD_TILE_SIZE * FIX_SCALE);
            int min_y = entity->pos.y / (WORLD_TILE_SIZE * FIX_SCALE);
            int max_x = (entity->pos.x + col_w) / (WORLD_TILE_SIZE * FIX_SCALE);
            int max_y = (entity->pos.y + col_h) / (WORLD_TILE_SIZE * FIX_SCALE);

            // mgba_printf(MGBA_LOG_DEBUG, "min_x: %i, max_x: %i", min_x, max_x);

            bool col_found = false;
            FIXED final_nx = 0, final_ny = 0, final_pd = 0;
            for (int y = min_y; y <= max_y; ++y)
            {
                for (int x = min_x; x <= max_x; ++x)
                {
                    if (map_collision_get(map_collision, gfx_map_width, x, y) != 1) continue;
                    
                    FIXED nx, ny, pd;
                    if (rect_collision(entity->pos.x, entity->pos.y, col_w,
                                       col_h, int2fx(x * WORLD_TILE_SIZE),
                                       int2fx(y * WORLD_TILE_SIZE),
                                       int2fx(WORLD_TILE_SIZE),
                                       int2fx(WORLD_TILE_SIZE), &nx, &ny, &pd))
                    {
                        if (pd > final_pd)
                        {
                            final_pd = pd;
                            final_nx = nx;
                            final_ny = ny;
                            col_found = true;
                        }
                    }
                }
            }

            if (col_found)
            {
                FIXED pdot = fxmul(final_nx, entity->vel.x) +
                             fxmul(final_ny, entity->vel.y);
                
                if (pdot < 0)
                {
                    FIXED px = fxmul(final_nx, final_pd);
                    FIXED py = fxmul(final_ny, final_pd);
                    entity->pos.x += px;
                    entity->pos.y += py;

                                
                    entity->vel.x -= fxmul(final_nx, pdot);
                    entity->vel.y -= fxmul(final_ny, pdot);

                    if (py < 0)
                        actor_flags |= ACTOR_FLAG_GROUNDED;
                }
            }

            entity->actor.flags = actor_flags;
        }
    }
}

int main()
{
    mgba_console_open();
    mgba_printf(MGBA_LOG_DEBUG, "Hello, world!");

    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    gfx_init();

    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, player_gfxTiles, 32 * 8);

    gfx_load_map(1);

    OBJ_ATTR *obj = &gfx_oam_buffer[0];
    obj_set_attr(obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK(0));
    int cam_x = 0;
    int cam_y = 0;

    map_collision = map_collision_data(gfx_loaded_map);

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_init(&entities[i]);
    }

    entity_s *player = &entities[0];
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

        update_entities();

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
