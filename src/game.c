#include "game.h"

#include <tonc_core.h>
#include <tonc_math.h>

#define MAP_COL(x, y)                                                          \
    map_collision_get(game_room_collision, game_room_width, x, y)

entity_s game_entities[MAX_ENTITY_COUNT];
const u8 *game_room_collision;
int game_room_width;
int game_room_height;

static uint col_contact_count = 0;
static col_contact_s col_contacts[MAX_CONTACT_COUNT];

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
        entity_s *entity = game_entities + i;
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
                    if (MAP_COL(x, y) != 1) continue;
                    
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

void game_init(void)
{
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_init(&game_entities[i]);
    }
}

void game_update(void)
{
    update_entities();
}

void game_load_room(const map_header_s *map)
{
    game_room_collision = map_collision_data(map);
    game_room_width = (int) map->width;
    game_room_height = (int) map->height;
}