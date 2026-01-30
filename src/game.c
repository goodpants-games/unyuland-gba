#include "game.h"

#include <tonc_core.h>
#include <tonc_math.h>
#include <stdlib.h>

#include <tonc_bios.h>

#include "datastruct.h"
#include "log.h"

#define MAP_COL(x, y)                                                          \
    map_collision_get(game_room_collision, game_room_width, x, y)

// x always has to be greater than y
#define UPAIR2U(x, y) ((((x) * (x) + (x)) >> 1) + (y))
#define CEIL_DIV(x, width) (((x) + (width) - 1) / (width))
#define IALIGN(x, width) (((x) + (width) - 1) / (width) * (width))

#define ENTITY_PAIR_SIZE \
    IALIGN(CEIL_DIV(UPAIR2U(MAX_ENTITY_COUNT - 1, MAX_ENTITY_COUNT - 1), 8), 4)
#define ENTITY_PAIR_GET(pairs, pkey) \
    ((pairs[(pkey) >> 3] >> ((pkey) & 0x7)) & 0x1)
#define ENTITY_PAIR_SET(pairs, pkey) \
    (pairs[(pkey) >> 3] |= (1 << ((pkey) & 0x7)))
#define ENTITY_PAIR_CLEAR(pairs, pkey) \
    (pairs[(pkey) >> 3] &= ~(1 << ((pkey) & 0x7)))

typedef struct entity_coldata {
    entity_s *ent;
    FIXED inv_mass;
} entity_coldata_s;

typedef struct col_contact {
    FIXED nx, ny, pd;
    entity_coldata_s *ent_a, *ent_b;
} col_contact_s;

//                    entity broadphase: sweep and prune
// - for the X and Y axes, store a sorted edge list containing the edges of each
//   collidable entity.
// - for the X and Y axes, store a bitfield with each bit describing if two
//   entities are colliding. refer to the upair2u function.
// - (only for the X axis) store a separate contiguous array which stores
//   broadphase overlap entries. each array entry contains references to both
//   entities.
// - when updating the edge list, if a new overlap was detected, set the
//   bitfield entry to 1 and add it to the overlap list. if a overlap was
//   destroyed, set the bitfield entry to 0, do a linear search of the overlap.
//   list with the pertinent entities and remove that overlap, shifting the
//   entries afterwards backwards to fill the slot.
// - since the Y axis does not have an overlap list, simply do not do anything
//   pertaining to such on that axis.
// - contact detection will therefore be fast; it's just going through the X
//   overlap list, and checking if the overlap exists on the Y axis bitfield. if
//   it does, do narrow-phase collision detection.
//
//               projectile broadphase: spatial partitioning
// the original unyuland used sweep and prune for both regular entities and
// projectiles. projectiles were also considered normal entities. but i feel
// that for this gba port, that will not suffice for memory or performance, for
// two reasons:
//   1) it is not possible for projectiles to store as much data as an entity
//      does, as projectiles only need to store position, velocity, sprite id,
//      and projectile kind. this is relevant because one room in unyuland can
//      have at least 80 projectiles active at once. that's a lot!
//   2) sweep and prune is not very optimal for projectiles, since they move
//      fast, are created and destroyed very frequently, and axis overlaps
//      between projectiles can change very frequently.
// thus what i propose is that projectiles be stored in a pool separate from
// entities and with a separate, more lean data structure. and additionally,
// that spatial partitioning will be used as the broadphase optimization for
// them.
//
// each partition cell will be 8x8 tiles in size. the grid will also have 8
// columns of cells and 8 rows of cells, making the maximum room size 64 tiles
// on any axis. fortunately, no rooms exceed this size. each cell stores a
// pointer to the root of a singly-linked list describing which projectiles
// occupy the cell. each linked list node is allocated in a pool.
//
// entity/projectile intersection will be handled by the entity collision code.
// 
// on update:
//   for each projectile:
//     store old partition-grid bounds
//
//     move projectile
//     if projectile is touching a tile:
//       destroy projectile
//
//     compute new partition-grid bounds
//     if grid bounds has changed:
//       remove entries referencing self in the old partition cells
//       add entries in new intersecting cells

typedef struct col_bp_edge {
    u16 eid;
    bool right;
    int pos;
} col_bp_edge_s;

entity_s game_entities[MAX_ENTITY_COUNT];
const u8 *game_room_collision;
int game_room_width;
int game_room_height;

static uint col_contact_count = 0;
static col_contact_s col_contacts[MAX_CONTACT_COUNT];

// static u8 x_overlap_pairs[ENTITY_PAIR_SIZE];
// static u8 y_overlap_pairs[ENTITY_PAIR_SIZE];

typedef struct col_overlap_res {
    bool overlap;
    FIXED nx, ny, pd;
} col_overlap_res_s;

static col_overlap_res_s rect_collision(FIXED x0, FIXED y0, FIXED w0, FIXED h0,
                                        FIXED x1, FIXED y1, FIXED w1, FIXED h1)
{
    const FIXED l0 = x0;
    const FIXED t0 = y0;
    const FIXED r0 = x0 + w0;
    const FIXED b0 = y0 + h0;

    const FIXED l1 = x1;
    const FIXED t1 = y1;
    const FIXED r1 = x1 + w1;
    const FIXED b1 = y1 + h1;

    col_overlap_res_s res;
    res.overlap = false;

    FIXED pl = r0 - l1;
    FIXED pt = b0 - t1;
    FIXED pr = r1 - l0;
    FIXED pb = b1 - t0;
    FIXED mp = min(pr, min(pt, min(pl, pb)));
    if (mp <= 0)
        return res;

    if (mp == pl)
    {
        res.nx = int2fx(-1);
        res.ny = int2fx(0);
    }
    else if (mp == pt)
    {
        res.nx = int2fx(0);
        res.ny = int2fx(-1);
    }
    else if (mp == pr)
    {
        res.nx = int2fx(1);
        res.ny = int2fx(0);
    }
    else if (mp == pb)
    {
        res.nx = int2fx(0);
        res.ny = int2fx(1);
    }
    else return res;

    res.pd = -mp;
    res.overlap = true;
    return res;
}

// unordered pairing function of two unsigned integers
static inline uint upair2u(uint a, uint b)
{
    uint x, y;
    if (a < b) x = a, y = b;
    else       x = b, y = a;
    return ((x * x + x) >> 1) + y;
}

entity_s* entity_alloc(void)
{
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *ent = game_entities + i;
        if (ent->flags & ENTITY_FLAG_ENABLED) continue;

        *ent = (entity_s) {
            .flags = ENTITY_FLAG_ENABLED,
            .gmult = FIX_ONE,
            .mass = 2,
            .col.group = COLGROUP_DEFAULT,
            .col.mask = COLGROUP_ALL
        };
        return ent;
    }

    LOG_ERR("entity pool is full!");
    return NULL;
}

void entity_free(entity_s *entity)
{
    entity->flags = 0;
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

        if (entity->behavior && entity->behavior->update)
        {
            entity->behavior->update(entity);
        }

        if (entity->flags & ENTITY_FLAG_ACTOR)
        {
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
            FIXED g = fxmul(gravity, entity->gmult);
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

static bool physics_substep(entity_coldata_s *col_ents, int col_ent_count,
                            FIXED vel_mult)
{
    size_t contact_queue_size = 0;

    static pqueue_entry_s contact_queue[MAX_CONTACT_COUNT];
    static u8 contact_pairs[ENTITY_PAIR_SIZE];

    // first move all entities
    for (int i = 0; i < col_ent_count; ++i)
    {
        entity_coldata_s *const col_ent = col_ents + i;
        entity_s *entity = col_ent->ent;

        FIXED s_vx = fxmul(entity->vel.x, vel_mult);
        FIXED s_vy = fxmul(entity->vel.y, vel_mult);
        entity->pos.x += s_vx;
        entity->pos.y += s_vy;
    }

    #ifdef PHYS_PROFILE
    u32 detection_time = 0;
    u32 resolution_time = 0;
    #endif
    
    for (int subsubstep = 1;; ++subsubstep)
    {
        if (subsubstep >= 8)
        {
            // LOG_WRN("exceeded max iterations");
            break;
        }

        col_contact_count = 0;

        // clear contact pair flags
        memset32(contact_pairs, 0, sizeof(contact_pairs) / 4);

        #ifdef PHYS_PROFILE
        profile_start();
        #endif

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
            // const uint col_group = (uint)entity->col.group;
            const uint col_mask = (uint)entity->col.mask;

            // entity contacts
            for (int j = 0; j < col_ent_count; ++j)
            {
                entity_coldata_s *const entc2 = col_ents + j;
                if (col_ent == entc2) continue;

                uint pkey = upair2u(i, j);
                
                if (ENTITY_PAIR_GET(contact_pairs, pkey))
                    continue;

                entity_s *ent2 = entc2->ent;
                const FIXED c2_w = int2fx((int)ent2->col.w);
                const FIXED c2_h = int2fx((int)ent2->col.h);
                
                col_overlap_res_s overlap_res =
                    rect_collision(entity->pos.x, entity->pos.y, col_w, col_h,
                                   ent2->pos.x, ent2->pos.y, c2_w, c2_h);

                if (overlap_res.overlap)
                {
                    ENTITY_PAIR_SET(contact_pairs, pkey);
                    // pqueue_enqueue(contact_queue,
                    //                &contact_queue_size, MAX_CONTACT_COUNT,
                    //                col_contacts + col_contact_count, pd);

                    col_contacts[col_contact_count] = (col_contact_s)
                    {
                        .nx = overlap_res.nx,
                        .ny = overlap_res.ny,
                        .pd = overlap_res.pd,
                        .ent_a = col_ent,
                        .ent_b = entc2
                    };
                    ++col_contact_count;
                }
            }

            // tile contacts
            if (col_mask & COLGROUP_DEFAULT)
            {
                int min_x = (int)((uint)entity->pos.x / (WORLD_TILE_SIZE * FIX_SCALE));
                int min_y = (int)((uint)entity->pos.y / (WORLD_TILE_SIZE * FIX_SCALE));
                int max_x = (int)((uint)(entity->pos.x + col_w) / (WORLD_TILE_SIZE * FIX_SCALE));
                int max_y = (int)((uint)(entity->pos.y + col_h) / (WORLD_TILE_SIZE * FIX_SCALE));

                bool tile_col_found = false;
                FIXED final_nx = 0, final_ny = 0, final_pd = 0;
                for (int y = min_y; y <= max_y; ++y)
                {
                    for (int x = min_x; x <= max_x; ++x)
                    {
                        if (MAP_COL((uint) x, (uint)y) != 1) continue;
                        
                        col_overlap_res_s overlap_res = 
                            rect_collision(entity->pos.x, entity->pos.y, col_w,
                                        col_h, int2fx(x * WORLD_TILE_SIZE),
                                        int2fx(y * WORLD_TILE_SIZE),
                                        int2fx(WORLD_TILE_SIZE),
                                        int2fx(WORLD_TILE_SIZE));
                        
                        if (overlap_res.overlap && overlap_res.pd < final_pd)
                        {
                            final_pd = overlap_res.pd;
                            final_nx = overlap_res.nx;
                            final_ny = overlap_res.ny;
                            tile_col_found = true;
                        }
                    }
                }

                if (tile_col_found)
                {
                    // pqueue_enqueue(contact_queue,
                    //                &contact_queue_size, MAX_CONTACT_COUNT,
                    //                col_contacts + col_contact_count, final_pd);

                    col_contacts[col_contact_count] = (col_contact_s)
                    {
                        .nx = final_nx,
                        .ny = final_ny,
                        .pd = final_pd,
                        .ent_a = col_ent,
                        .ent_b = NULL
                    };
                    ++col_contact_count;
                }
            }
        }

        #ifdef PHYS_PROFILE
        detection_time += profile_stop();
        #endif

        if (col_contact_count == 0)
        {
            break;
        }

        #ifdef PHYS_PROFILE
        profile_start();
        #endif

        // resolve contacts
        // for (void *item;
        //      (item = pqueue_dequeue(contact_queue, &contact_queue_size));)
        for (int i = 0; i < col_contact_count; ++i)
        {
            col_contact_s *const contact = col_contacts + i;
            entity_coldata_s *const col_ent_a = contact->ent_a;
            entity_coldata_s *const col_ent_b = contact->ent_b;
            
            FIXED nx = contact->nx;
            FIXED ny = contact->ny;
            FIXED pd = contact->pd;

            entity_s *const ent_a = col_ent_a->ent;
            entity_s *const ent_b = col_ent_b ? col_ent_b->ent : NULL;

            FIXED rel_vx, rel_vy;
            if (ent_b)
            {
                rel_vx = ent_a->vel.x - ent_b->vel.x;
                rel_vy = ent_a->vel.y - ent_b->vel.y;
            }
            else
            {
                rel_vx = ent_a->vel.x;
                rel_vy = ent_a->vel.y;
            }

            FIXED vdot = fxmul(nx, rel_vx) +
                         fxmul(ny, rel_vy);
                
            if (vdot <= 0)
            {
                if (col_ent_b && ent_b->flags & ENTITY_FLAG_MOVING)
                {
                    FIXED inv_mass1 = col_ent_a->inv_mass;
                    FIXED inv_mass2 = col_ent_b->inv_mass;

                    FIXED total_inv_mass = inv_mass1 + inv_mass2;
                    if (total_inv_mass == 0) continue;
                    FIXED inv_total_inv_mass = fxdiv(FIX_ONE, total_inv_mass);

                    FIXED restitution = (FIXED)(FIX_ONE * (1 + 0.5));
                    FIXED move_x = fxmul(fxmul(nx, pd), inv_total_inv_mass);
                    FIXED move_y = fxmul(fxmul(ny, pd), inv_total_inv_mass);
                    FIXED impulse_fac = fxmul(fxmul(restitution, vdot), inv_total_inv_mass);

                    ent_a->pos.x -= fxmul(move_x, inv_mass1);
                    ent_a->pos.y -= fxmul(move_y, inv_mass1);
                    ent_b->pos.x += fxmul(move_x, inv_mass2);
                    ent_b->pos.y += fxmul(move_y, inv_mass2);

                    ent_a->vel.x -= fxmul(fxmul(nx, impulse_fac), inv_mass1);
                    ent_a->vel.y -= fxmul(fxmul(ny, impulse_fac), inv_mass1);
                    ent_b->vel.x += fxmul(fxmul(nx, impulse_fac), inv_mass2);
                    ent_b->vel.y += fxmul(fxmul(ny, impulse_fac), inv_mass2);

                    if (ny < 0)
                        ent_a->actor.flags |= ACTOR_FLAG_GROUNDED;
                    else if (ny > 0)
                        ent_b->actor.flags |= ACTOR_FLAG_GROUNDED;
                }
                else
                {   
                    FIXED px = fxmul(nx, pd);
                    FIXED py = fxmul(ny, pd);

                    ent_a->pos.x = ent_a->pos.x - px;
                    ent_a->pos.y = ent_a->pos.y - py;
                    ent_a->vel.x = ent_a->vel.x - fxmul(nx, vdot);
                    ent_a->vel.y = ent_a->vel.y - fxmul(ny, vdot);

                    if (ny < 0)
                        ent_a->actor.flags |= ACTOR_FLAG_GROUNDED;
                }
            }
        }

        #ifdef PHYS_PROFILE
        resolution_time += profile_stop();
        #endif
    }

    #ifdef PHYS_PROFILE
    LOG_DBG("detection time: %.2f%%", (float)detection_time / 280896.f * 100.f);
    LOG_DBG("resolution time: %.2f%%", (float)resolution_time / 280896.f * 100.f);
    #endif

    bool no_movement = true;

    for (int i = 0; i < col_ent_count; ++i)
    {
        entity_s *entity = col_ents[i].ent;
        if (entity->vel.x != 0 || entity->vel.y != 0)
        {
            no_movement = false;
            break;
        }
    }

    return no_movement;
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

        if (entity->flags & ENTITY_FLAG_ACTOR)
            entity->actor.flags &= ~(ACTOR_FLAG_GROUNDED | ACTOR_FLAG_WALL |
                                    ACTOR_FLAG_DID_JUMP);

        if (!(entity->flags & ENTITY_FLAG_COLLIDE)) continue;
        
        col_ents[col_ent_count++] = (entity_coldata_s)
        {
            .ent = entity,
            .inv_mass = fxdiv(FIX_ONE * 2, int2fx((int)entity->mass))
        };

        int speed = max(abs(entity->vel.x), abs(entity->vel.y));
        int subst = ceil_div(speed, FIX_ONE * 2);
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

    FIXED vel_mult = FIX_ONE / substeps;
    for (int i = 0; i < substeps; ++i)
    {
        if (physics_substep(col_ents, col_ent_count, vel_mult)) break;
    }
}

void game_init(void)
{
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        game_entities[i].flags = 0;
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