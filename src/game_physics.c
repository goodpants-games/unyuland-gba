#include <stdlib.h>

#include "game.h"
#include "datastruct.h"
#include "math_util.h"
#include "log.h"


#define ENTITY_PAIR_SIZE \
    IALIGN(CEIL_DIV(UPAIR2U(MAX_ENTITY_COUNT - 1, MAX_ENTITY_COUNT - 1), 8), 4)
#define ENTITY_PAIR_GET(pairs, pkey) \
    ((pairs[(pkey) >> 3] >> ((pkey) & 0x7)) & 0x1)
#define ENTITY_PAIR_SET(pairs, pkey) \
    (pairs[(pkey) >> 3] |= (1 << ((pkey) & 0x7)))
#define ENTITY_PAIR_CLEAR(pairs, pkey) \
    (pairs[(pkey) >> 3] &= ~(1 << ((pkey) & 0x7)))

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

typedef struct col_bp_edge
{
    u16 eid;
    bool right;
    int pos;
}
col_bp_edge_s;

typedef struct col_overlap_res {
    bool overlap;
    FIXED nx, ny, pd;
} col_overlap_res_s;

static uint col_contact_count = 0;
static col_contact_s col_contacts[MAX_CONTACT_COUNT];

// static u8 x_overlap_pairs[ENTITY_PAIR_SIZE];
// static u8 y_overlap_pairs[ENTITY_PAIR_SIZE];

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
            
            if (vdot > 0) continue;

            uint col_group_a = (uint)ent_a->col.group;
            uint col_mask_a = (uint)ent_a->col.mask;

            uint col_group_b, col_mask_b;
            if (ent_b)
            {
                col_group_b = (uint)ent_b->col.group;
                col_mask_b = (uint)ent_b->col.mask;
            }
            else
            {
                col_group_b = COLGROUP_DEFAULT;
                col_mask_b = COLGROUP_ALL;
            }

            if (!(col_group_b & col_mask_a) && !(col_group_a & col_mask_b))
                continue;
            
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

                if (nx != 0)
                {
                    ent_a->actor.flags |= ACTOR_FLAG_WALL;
                    ent_b->actor.flags |= ACTOR_FLAG_WALL;
                }
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

                if (nx != 0)
                    ent_a->actor.flags |= ACTOR_FLAG_WALL;
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

void game_physics_update(void)
{
    col_contact_count = 0;

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