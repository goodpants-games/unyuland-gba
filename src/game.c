#include "game.h"

#include <stdlib.h>
#include <tonc.h>

#include "datastruct.h"
#include "log.h"
#include "gfx.h"
#include "math_util.h"

#define ENTITY_PAIR_SIZE \
    IALIGN(CEIL_DIV(UPAIR2U(MAX_ENTITY_COUNT - 1, MAX_ENTITY_COUNT - 1), 8), 4)
#define ENTITY_PAIR_GET(pairs, pkey) \
    ((pairs[(pkey) >> 3] >> ((pkey) & 0x7)) & 0x1)
#define ENTITY_PAIR_SET(pairs, pkey) \
    (pairs[(pkey) >> 3] |= (1 << ((pkey) & 0x7)))
#define ENTITY_PAIR_CLEAR(pairs, pkey) \
    (pairs[(pkey) >> 3] &= ~(1 << ((pkey) & 0x7)))

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

typedef struct entity_coldata
{
    entity_s *ent;
    FIXED inv_mass;
}
entity_coldata_s;

typedef struct col_contact
{
    FIXED nx, ny, pd;
    entity_coldata_s *ent_a, *ent_b;
}
col_contact_s;

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

game_s g_game;

static uint col_contact_count = 0;
static col_contact_s col_contacts[MAX_CONTACT_COUNT];

static uint sprite_render_arr_size = 0;
static entity_s *sprite_render_arr[MAX_ENTITY_COUNT];

// static u8 x_overlap_pairs[ENTITY_PAIR_SIZE];
// static u8 y_overlap_pairs[ENTITY_PAIR_SIZE];

typedef struct col_overlap_res {
    bool overlap;
    FIXED nx, ny, pd;
} col_overlap_res_s;

static inline int map_col_get_bounded(int x, int y)
{
    if (x < 0)
        x = 0;
    else if (x >= g_game.room_width)
        x = g_game.room_width - 1;

    if (y < 0)
        y = 0;
    else if (y >= g_game.room_height)
        y = g_game.room_height - 1;

    return map_collision_get(g_game.room_collision, g_game.room_width, x, y);
}

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

        sprite_render_arr[sprite_render_arr_size++] = ent;
        return ent;
    }

    LOG_ERR("entity pool is full!");
    return NULL;
}

void entity_free(entity_s *entity)
{
    entity->flags = 0;

    // remove from render list
    for (int i = 0; i < sprite_render_arr_size; ++i)
    {
        if (sprite_render_arr[i] == entity)
        {
            int end = sprite_render_arr_size - 1;
            for (int j = i; j < end; ++j)
            {
                sprite_render_arr[j] = sprite_render_arr[j+1];
            }
            --sprite_render_arr_size;
            return;
        }
    }

    LOG_ERR("entity_free: could not find entity in render list");
}

static void update_entities(void)
{
    const FIXED gravity = float2fx(0.1f);
    const FIXED terminal_vel = int2fx(5);

    col_contact_count = 0;

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

            if (!(entity->flags & ENTITY_FLAG_MOVING)) continue;

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

                if (!overlap_res.overlap) continue;
                if (overlap_res.ny >= 0 && ent2->col.flags & COL_FLAG_FLOOR_ONLY)
                    continue;
                if (overlap_res.ny <= 0 && entity->col.flags & COL_FLAG_FLOOR_ONLY)
                    continue;

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

            // tile contacts
            if (col_mask & COLGROUP_DEFAULT)
            {
                const int el = entity->pos.x;
                const int et = entity->pos.y;
                const int er = (entity->pos.x + col_w);
                const int eb = (entity->pos.y + col_h);

                int min_x = el / (WORLD_TILE_SIZE * FIX_SCALE);
                int min_y = et / (WORLD_TILE_SIZE * FIX_SCALE);
                int max_x = er / (WORLD_TILE_SIZE * FIX_SCALE);
                int max_y = eb / (WORLD_TILE_SIZE * FIX_SCALE);
                
                // integer division rounds towards zero, but i want to round
                // towards negative infinity (floor). this should fix it.
                // micro-optimization notice: doing `a / b - (a < 0)` is faster,
                // but it incurs a cost for both positive and negative values.
                // most of the time, the value will be positive. therefore, it
                // is generally faster to branch.
                // (at least on -O2)
                if (el < 0) --min_x;
                if (et < 0) --min_y;
                if (er < 0) --min_x;
                if (eb < 0) --min_y;

                bool tile_col_found = false;
                FIXED final_nx = 0, final_ny = 0, final_pd = 0;
                for (int y = min_y; y <= max_y; ++y)
                {
                    for (int x = min_x; x <= max_x; ++x)
                    {
                        if (map_col_get_bounded(x, y) != 1) continue;
                        
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

                    FIXED restitution = TO_FIXED(1.0 + 0.5);
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

static void update_physics(void)
{
    static entity_coldata_s col_ents[MAX_ENTITY_COUNT];
    int col_ent_count = 0;
    int substeps = 0;

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *entity = g_game.entities + i;
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
    g_game.cam_x = 0;
    g_game.cam_y = 0;
    g_game.input_enabled = true;
    
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        g_game.entities[i].flags = 0;
    }
}

void game_update(void)
{
    update_entities();
    update_physics();
}

void game_load_room(const map_header_s *map)
{
    g_game.room_collision = map_collision_data(map);
    g_game.room_width = (int) map->width;
    g_game.room_height = (int) map->height;
}

void game_render(int *p_last_obj_index)
{
    // sort render list by z index
    for (int i = 1; i < sprite_render_arr_size; ++i)
    {
        for (int j = i; j > 0; --j)
        {
            if (sprite_render_arr[j]->sprite.zidx > sprite_render_arr[j-1]->sprite.zidx)
            {
                entity_s *temp = sprite_render_arr[j];
                sprite_render_arr[j] = sprite_render_arr[j-1];
                sprite_render_arr[j-1] = temp;
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
    for (int i = 0; i < sprite_render_arr_size; ++i)
    {
        entity_s *ent = sprite_render_arr[i];
        if (!(ent->flags & ENTITY_FLAG_ENABLED)) continue;

        int draw_x = (ent->pos.x >> FIX_SHIFT) + ent->sprite.ox;
        int draw_y = (ent->pos.y >> FIX_SHIFT) + ent->sprite.oy;
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

        int sprite_frame = ent->sprite.frame;
        int sprite_time_accum = ent->sprite.accum;
        bool sprite_hflip = ent->sprite.flags & SPRITE_FLAG_FLIP_X;
        bool sprite_vflip = ent->sprite.flags & SPRITE_FLAG_FLIP_Y;

        const gfx_sprite_s *spr = &gfx_sprites[ent->sprite.graphic_id];
        const gfx_frame_s *frame = frame_pool + spr->frame_pool_idx + sprite_frame;
        const gfx_obj_s *objs = obj_pool + frame->obj_pool_index;

        int frame_count = spr->frame_count;
        int frame_len = frame->frame_len;
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

        // update sprite animation
        if ((ent->sprite.flags & SPRITE_FLAG_PLAYING) && ++sprite_time_accum >= frame_len)
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

    exit_entity_loop:;

    int last_obj_index = *p_last_obj_index;
    for (int i = obj_index; i < last_obj_index; ++i)
    {
        obj_hide(&gfx_oam_buffer[i]);
    }

    *p_last_obj_index = obj_index;
}