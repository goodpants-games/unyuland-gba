#include "game.h"

#include <tonc_core.h>
#include <tonc_math.h>
#include <stdlib.h>

#include "datastruct.h"
#include "log.h"

#define MAP_COL(x, y)                                                          \
    map_collision_get(game_room_collision, game_room_width, x, y)

typedef struct entity_coldata {
    entity_s *ent;
    bool processed;
} entity_coldata_s;

typedef struct col_contact {
    FIXED nx, ny, pd;
    entity_coldata_s *ent_a, *ent_b;
} col_contact_s;

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
    *self = (entity_s) {
        .gmult = 255,
        .mass = 2,
        .col.group = COLGROUP_DEFAULT,
        .col.mask = COLGROUP_ALL
    };
}

static void update_entities(void)
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

            // if this entity can collide, movement is done in update-physics
            if (!(entity->flags & ENTITY_FLAG_COLLIDE))
            {
                entity->pos.x += entity->vel.x;
                entity->pos.y += entity->vel.y;
            }
        }
    }
}

static void physics_substep(entity_coldata_s *col_ents, int col_ent_count,
                            FIXED vel_mult)
{
    size_t contact_queue_size = 0;
    pqueue_entry_s contact_queue[MAX_CONTACT_COUNT];

    // first move all entities
    for (int i = 0; i < col_ent_count; ++i)
    {
        entity_coldata_s *const col_ent = col_ents + i;
        entity_s *entity = col_ent->ent;

        entity->pos.x += fxmul(entity->vel.x, vel_mult);
        entity->pos.y += fxmul(entity->vel.y, vel_mult);

        entity->actor.flags &= ~(ACTOR_FLAG_GROUNDED | ACTOR_FLAG_WALL |
                                 ACTOR_FLAG_DID_JUMP);
    }

    for (int subsubstep = 1; subsubstep < 4; ++subsubstep)
    {
        col_contact_count = 0;

        // reset movement state
        for (int i = 0; i < col_ent_count; ++i)
        {
            entity_coldata_s *const col_ent = col_ents + i;
            col_ent->processed = false;
        }

        // collect contacts
        for (int i = 0; i < col_ent_count; ++i)
        {
            if (col_contact_count >= MAX_CONTACT_COUNT)
            {
                LOG_WRN("max contacts exceeded!");
                break;
            }

            entity_coldata_s *const col_ent = col_ents + i;
            entity_s *entity = col_ent->ent;

            const FIXED col_w = int2fx((int)entity->col.w);
            const FIXED col_h = int2fx((int)entity->col.h);

            // entity contacts
            for (int j = 0; j < col_ent_count; ++j)
            {
                entity_coldata_s *const entc2 = col_ents + j;
                if (col_ent == entc2) continue;

                entity_s *ent2 = entc2->ent;
                FIXED nx, ny, pd;
                const FIXED c2_w = int2fx((int)ent2->col.w);
                const FIXED c2_h = int2fx((int)ent2->col.h);

                if (rect_collision(entity->pos.x, entity->pos.y, col_w, col_h,
                                   ent2->pos.x, ent2->pos.y, c2_w, c2_h,
                                   &nx, &ny, &pd))
                {
                    pqueue_enqueue(contact_queue,
                                   &contact_queue_size, MAX_CONTACT_COUNT,
                                   (pqueue_entry_s) {
                                       .priority = pd,
                                       .data = col_contacts + col_contact_count
                                   });

                    col_contacts[col_contact_count++] = (col_contact_s)
                    {
                        .nx = nx,
                        .ny = ny,
                        .pd = pd,
                        .ent_a = col_ent,
                        .ent_b = entc2
                    };
                }
            }

            // tile contacts
            int min_x = entity->pos.x / (WORLD_TILE_SIZE * FIX_SCALE);
            int min_y = entity->pos.y / (WORLD_TILE_SIZE * FIX_SCALE);
            int max_x = (entity->pos.x + col_w) / (WORLD_TILE_SIZE * FIX_SCALE);
            int max_y = (entity->pos.y + col_h) / (WORLD_TILE_SIZE * FIX_SCALE);

            bool tile_col_found = false;
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
                            tile_col_found = true;
                        }
                    }
                }
            }

            if (tile_col_found)
            {
                pqueue_enqueue(contact_queue,
                               &contact_queue_size, MAX_CONTACT_COUNT,
                               (pqueue_entry_s) {
                                   .priority = final_pd,
                                   .data = col_contacts + col_contact_count
                               });

                col_contacts[col_contact_count++] = (col_contact_s)
                {
                    .nx = final_nx,
                    .ny = final_ny,
                    .pd = final_pd,
                    .ent_a = col_ent,
                    .ent_b = NULL
                };
            }
        }

        if (col_contact_count == 0) break;

        for (void *item;
             (item = pqueue_dequeue(contact_queue, &contact_queue_size));)
        {
            col_contact_s *const contact = (col_contact_s *)item;
            entity_coldata_s *const col_ent_a = contact->ent_a;
            entity_coldata_s *const col_ent_b = contact->ent_b;

            if (col_ent_a->processed || (col_ent_b && col_ent_b->processed))
                continue;
            
            col_ent_a->processed = true;
            if (col_ent_b)
                col_ent_b->processed = true;
            
            FIXED nx = contact->nx;
            FIXED ny = contact->ny;
            FIXED pd = contact->pd;

            entity_s *const ent_a = col_ent_a->ent;
            entity_s *const ent_b = col_ent_b ? col_ent_b->ent : NULL;

            FIXED vdot = fxmul(nx, ent_a->vel.x) +
                        fxmul(ny, ent_a->vel.y);
                
            if (vdot < 0)
            {
                if (col_ent_b)
                {
                    nx = fxmul(nx, FIX_SCALE / 2);
                    ny = fxmul(ny, FIX_SCALE / 2);
                    FIXED px = fxmul(nx, pd);
                    FIXED py = fxmul(ny, pd);

                    ent_a->pos.x += px;
                    ent_a->pos.y += py;
                    ent_a->vel.x -= fxmul(nx, vdot);
                    ent_a->vel.y -= fxmul(ny, vdot);

                    ent_b->pos.x -= px;
                    ent_b->pos.y -= py;
                    ent_b->vel.x += fxmul(nx, vdot);
                    ent_b->vel.y += fxmul(ny, vdot);
                }
                else
                {
                    FIXED px = fxmul(nx, pd);
                    FIXED py = fxmul(ny, pd);
                    ent_a->pos.x += px;
                    ent_a->pos.y += py;
                    ent_a->vel.x -= fxmul(nx, vdot);
                    ent_a->vel.y -= fxmul(ny, vdot);

                    if (py < 0)
                        ent_a->actor.flags |= ACTOR_FLAG_GROUNDED;
                }
            }
        }
    }
}

INLINE int ceil_div(int a, int b)
{
    return (a + b - 1) / b;
}

static void update_physics(void)
{
    static entity_coldata_s col_ents[MAX_ENTITY_COUNT];
    int col_ent_count = 0;
    int substeps = 0;

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *entity = game_entities + i;
        if (!(entity->flags & ENTITY_FLAG_ENABLED)) continue;

        col_ents[col_ent_count++] = (entity_coldata_s)
        {
            .ent = entity
        };

        int speed = max(abs(entity->vel.x), abs(entity->vel.y));
        // align is just a ceil division. so just use that lmao.
        int subst = ceil_div(speed, FIX_SCALE * 2);
        if (subst > substeps)
            substeps = subst;
    }

    // probably good if i snap the substep count to a power of two, so that
    // division is exact
    --substeps;
    substeps |= substeps >> 1;
    substeps |= substeps >> 2;
    substeps |= substeps >> 4;
    substeps |= substeps >> 8;
    substeps |= substeps >> 16;
    ++substeps;

    FIXED vel_mult = FIX_SCALE / substeps;
    for (int i = 0; i < substeps; ++i)
    {
        physics_substep(col_ents, col_ent_count, vel_mult);
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
    update_physics();
}

void game_load_room(const map_header_s *map)
{
    game_room_collision = map_collision_data(map);
    game_room_width = (int) map->width;
    game_room_height = (int) map->height;
}