#include <stdlib.h>

#include "game.h"
#include "game_physics.h"
#include "datastruct.h"
#include "math_util.h"
#include "log.h"
#include "tonc_math.h"

// in world units (8 units per tile)
#define PARTGRID_CEL_W 64
#define PARTGRID_CEL_H 64
#define PARTGRID_COLS  8
#define PARTGRID_ROWS  8
#define PARTGRID_NODE_POOL_SIZE 128

#define ENTITY_PAIR_SIZE \
    IALIGN(CEIL_DIV(UPAIR2U(MAX_ENTITY_COUNT - 1, MAX_ENTITY_COUNT - 1), 8), 4)
#define ENTITY_PAIR_GET(pairs, pkey) \
    ((pairs[(pkey) >> 3] >> ((pkey) & 0x7)) & 0x1)
#define ENTITY_PAIR_SET(pairs, pkey) \
    (pairs[(pkey) >> 3] |= (1 << ((pkey) & 0x7)))
#define ENTITY_PAIR_CLEAR(pairs, pkey) \
    (pairs[(pkey) >> 3] &= ~(1 << ((pkey) & 0x7)))

#define EDGE_LIST_MAX_COUNT ((MAX_ENTITY_COUNT * 2))

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
    bool left;
    FIXED pos;
}
col_bp_edge_s;

typedef struct col_bp_overlap
{
    u16 eid_a, eid_b;
}
col_bp_overlap_s;

typedef struct col_overlap_res {
    bool overlap;
    FIXED nx, ny, pd;
} col_overlap_res_s;

typedef struct partgrid_node
{
    projectile_s *projectile;

    // if active (i.e. projectile != NULL), this points to the next node
    // in the partition cell linked list. if inactive, this instead points to
    // the next unallocated node in the pool.
    union
    {
        struct partgrid_node *next;
        struct partgrid_node *next_free;
    };
} partgrid_node_s;

typedef partgrid_node_s *part_cell_t;

typedef struct proj_part_data
{
    part_cell_t *part_cell;
}
proj_part_data_s;

static int col_ent_count = 0;
static entity_coldata_s col_ent_map[MAX_ENTITY_COUNT];
static entity_coldata_s *col_ents[MAX_ENTITY_COUNT];

static uint col_contact_count = 0;
static col_contact_s col_contacts[MAX_CONTACT_COUNT];
// static u8 contact_pairs[ENTITY_PAIR_SIZE];

static int x_overlap_count = 0;
static col_bp_overlap_s x_overlaps[MAX_ENTITY_COUNT * 2];

static int x_edge_count = 0;
static col_bp_edge_s x_edges[EDGE_LIST_MAX_COUNT];

static int y_edge_count = 0;
static col_bp_edge_s y_edges[EDGE_LIST_MAX_COUNT];

static u8 x_contact_pairs[ENTITY_PAIR_SIZE];
static u8 y_contact_pairs[ENTITY_PAIR_SIZE];

static proj_part_data_s proj_data[MAX_PROJECTILE_COUNT];
static partgrid_node_s partgrid_node_pool[PARTGRID_NODE_POOL_SIZE];
static partgrid_node_s *partgrid_node_pool_ffree; // first free
static part_cell_t partgrid[PARTGRID_ROWS][PARTGRID_COLS];

// static pqueue_entry_s contact_queue[MAX_CONTACT_COUNT];

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

void game_physics_move_projs(FIXED vel_mult);










static void col_ent_added(int col_ent_idx)
{
    LOG_DBG("col_ent_added %i", col_ent_idx);

    x_edges[x_edge_count++] = (col_bp_edge_s)
    {
        .eid = (u16) col_ent_idx,
        .left = true
    };
    x_edges[x_edge_count++] = (col_bp_edge_s)
    {
        .eid = (u16) col_ent_idx,
        .left = false
    };

    y_edges[y_edge_count++] = (col_bp_edge_s)
    {
        .eid = (u16) col_ent_idx,
        .left = true
    };
    y_edges[y_edge_count++] = (col_bp_edge_s)
    {
        .eid = (u16) col_ent_idx,
        .left = false
    };
}
        

static void col_ent_removed(int col_ent_idx)
{
    LOG_DBG("col_ent_removed");

    // remove x edge
    for (int i = x_edge_count - 1; i >= 0; --i)
    {
        if (x_edges[i].eid == col_ent_idx)
            DYNARR_REMOVE(x_edges, x_edge_count, i);
    }

    // remove y edge
    for (int i = y_edge_count - 1; i >= 0; --i)
    {
        if (y_edges[i].eid == col_ent_idx)
            DYNARR_REMOVE(y_edges, y_edge_count, i);
    }
}

static void sort_edge_list(col_bp_edge_s *const list,
                           const int list_count,
                           u8 contact_pairs[ENTITY_PAIR_SIZE],
                           col_bp_overlap_s *overlaps, int *overlap_count)
{
    for (int i = 1; i < list_count; ++i)
    {
        int j = i - 1;
        while (list[j].pos > list[j+1].pos)
        {
            // LOG_DBG("swap %i%s %i%s",
            //         (int) list[j].eid, list[j].left ? "L" : "R",
            //         (int) list[j+1].eid, list[j+1].left ? "L" : "R");
            {
                col_bp_edge_s temp;
                SWAP3(list[j], list[j+1], temp);
            }

            col_bp_edge_s *const edge1 = &list[j];
            col_bp_edge_s *const edge2 = &list[j+1];

            int eid1 = edge1->eid;
            int eid2 = edge2->eid;

            uint pair_key = upair2u(eid1, eid2);

            // R-L -> L-R (add overlap)
            if (edge1->left && !edge2->left)
            {
                ENTITY_PAIR_SET(contact_pairs, pair_key);

                if (overlaps)
                {
                    // LOG_DBG("new X overlap %i %i", eid1, eid2);

                    // for (int k = 0; k < *overlap_count; ++k)
                    // {
                    //     if ((overlaps[k].eid_a == eid1 && overlaps[k].eid_b == eid2) ||
                    //         (overlaps[k].eid_a == eid2 && overlaps[k].eid_b == eid1))
                    //     {
                    //         LOG_ERR("overlap already exists in list!");
                    //         break;
                    //     }
                    // }

                    overlaps[(*overlap_count)++] = (col_bp_overlap_s)
                    {
                        .eid_a = eid1,
                        .eid_b = eid2
                    };
                }
            }
            // L-R -> R-L (remove overlap)
            else if (!edge1->left && edge2->left)
            {
                ENTITY_PAIR_CLEAR(contact_pairs, pair_key);

                if (overlaps)
                {
                    // LOG_DBG("delete X overlap %i %i", eid1, eid2);
                    int count = *overlap_count;
                    for (int k = 0; k < count; ++k)
                    {
                        if ((overlaps[k].eid_a == eid1 && overlaps[k].eid_b == eid2) ||
                            (overlaps[k].eid_a == eid2 && overlaps[k].eid_b == eid1))
                        {
                            DYNARR_REMOVE(overlaps, *overlap_count, k);
                            goto overlap_found;
                        }
                    }
                    LOG_ERR("overlap not found in list!");
                    overlap_found:;
                }
            }

            if (j == 0) break;
            --j;
        }
    }
}

static void update_edge_lists(void)
{
    // sync x edges
    for (int i = 0; i < x_edge_count; ++i)
    {
        col_bp_edge_s *edge = x_edges + i;
        const entity_coldata_s *col_ent = col_ent_map + edge->eid;
        const entity_s *ent = col_ent->ent;

        if (edge->left)
            edge->pos = ent->pos.x;
        else
            edge->pos = ent->pos.x + int2fx(ent->col.w);
    }

    // sync y edges
    for (int i = 0; i < y_edge_count; ++i)
    {
        col_bp_edge_s *edge = y_edges + i;
        const entity_coldata_s *col_ent = col_ent_map + edge->eid;
        const entity_s *ent = col_ent->ent;

        if (edge->left)
            edge->pos = ent->pos.y;
        else
            edge->pos = ent->pos.y + int2fx(ent->col.h);
    }

    sort_edge_list(x_edges, x_edge_count, x_contact_pairs, x_overlaps,
                   &x_overlap_count);
    sort_edge_list(y_edges, y_edge_count, y_contact_pairs, NULL, NULL);
}

static bool physics_substep(FIXED vel_mult)
{
    // size_t contact_queue_size = 0;

    // first move all entities
    for (int i = 0; i < col_ent_count; ++i)
    {
        entity_coldata_s *const col_ent = col_ents[i];
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
    
    // then, perform collision detection and resolution
    for (int subsubstep = 1;; ++subsubstep)
    {
        if (subsubstep >= 8)
        {
            // LOG_WRN("exceeded max iterations");
            break;
        }

        #ifdef PHYS_PROFILE
        profile_start();
        #endif

        update_edge_lists();
        col_contact_count = 0;

        // collect entity contacts
        for (int i = 0; i < x_overlap_count; ++i)
        {
            if (col_contact_count >= MAX_CONTACT_COUNT)
            {
                LOG_WRN("max contacts exceeded!");
                goto exit_contact_collection;
            }

            const col_bp_overlap_s *x_overlap = x_overlaps + i;
            uint pkey = upair2u(x_overlap->eid_a, x_overlap->eid_b);
            if (!ENTITY_PAIR_GET(y_contact_pairs, pkey)) continue;

            entity_coldata_s *col_ent = col_ent_map + x_overlap->eid_a;
            entity_coldata_s *entc2 = col_ent_map + x_overlap->eid_b;

            // make sure that the second is the one that isn't moving
            if (entc2->ent->flags & ENTITY_FLAG_MOVING)
            {
                entity_coldata_s *temp;
                SWAP3(col_ent, entc2, temp);
            }

            entity_s *entity = col_ent->ent;
            entity_s *ent2 = entc2->ent;

            // if both entities are static objects, collision cannot happen
            // between them.
            if (!(entity->flags & ENTITY_FLAG_MOVING) &&
                !(ent2->flags & ENTITY_FLAG_MOVING))
                continue;

            const FIXED col_w = int2fx((int)entity->col.w);
            const FIXED col_h = int2fx((int)entity->col.h);

            if (col_ent == entc2)
            {
                LOG_WRN("entity contact is with itself? how tf?");
                continue;
            }

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

            if (entity->behavior && entity->behavior->ent_touch)
                entity->behavior->ent_touch(entity, ent2);
            if (ent2->behavior && ent2->behavior->ent_touch)
                ent2->behavior->ent_touch(ent2, entity);
        }

        // collect tile contacts
        // also projectile detection and handling
        for (int i = 0; i < col_ent_count; ++i)
        {
            if (col_contact_count >= MAX_CONTACT_COUNT)
            {
                LOG_WRN("max contacts exceeded!");
                goto exit_contact_collection;
            }

            entity_coldata_s *const col_ent = col_ents[i];
            entity_s *entity = col_ent->ent;

            if (!(entity->flags & ENTITY_FLAG_MOVING)) continue;

            const FIXED col_w = int2fx((int)entity->col.w);
            const FIXED col_h = int2fx((int)entity->col.h);
            // const uint col_group = (uint)entity->col.group;
            const uint col_mask = (uint)entity->col.mask;

            const int el = entity->pos.x;
            const int et = entity->pos.y;
            const int er = (entity->pos.x + col_w);
            const int eb = (entity->pos.y + col_h);

            if (col_mask & COLGROUP_DEFAULT)
            {
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

            if (col_mask & COLGROUP_PROJECTILE)
            {
                int min_px = el / (FIX_ONE * PARTGRID_CEL_W);
                int min_py = et / (FIX_ONE * PARTGRID_CEL_H);
                int max_px = er / (FIX_ONE * PARTGRID_CEL_W);
                int max_py = eb / (FIX_ONE * PARTGRID_CEL_H);

                // clamp bounds to partition grid
                if      (min_px < 0)              min_px = 0;
                else if (min_px >= PARTGRID_COLS) min_px = PARTGRID_COLS - 1;
                if      (max_px < 0)              max_px = 0;
                else if (max_px >= PARTGRID_COLS) max_px = PARTGRID_COLS - 1;

                if      (min_py < 0)              min_py = 0;
                else if (min_py >= PARTGRID_ROWS) min_py = PARTGRID_ROWS - 1;
                if      (max_py < 0)              max_py = 0;
                else if (max_py >= PARTGRID_ROWS) max_py = PARTGRID_ROWS - 1;

                for (int y = min_py; y <= max_py; ++y)
                    for (int x = min_px; x <= max_px; ++x)
                    {
                        for (partgrid_node_s *node = partgrid[y][x]; node;
                            node = node->next)
                        {
                            projectile_s *proj = node->projectile;
                            if (proj->flags & PROJ_FLAG_QFREE) continue;

                            FIXED px = proj->px;
                            FIXED py = proj->py;

                            if (!(px > el && px < er && py > et && py < eb))
                                continue;

                            LOG_DBG("projectile collision!");

                            bool keep;
                            if (entity->behavior &&
                                entity->behavior->proj_touch)
                            {
                                keep = entity->behavior->proj_touch(entity, proj);
                            }
                            else
                            {
                                keep = false;
                            }
                            
                            if (!keep)
                                projectile_queue_free(proj);
                        }
                    }
            }

        }

        exit_contact_collection:;

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
        entity_s *entity = col_ents[i]->ent;
        if (entity->vel.x != 0 || entity->vel.y != 0)
        {
            no_movement = false;
            break;
        }
    }

    return no_movement;
}









static partgrid_node_s* partgrid_node_alloc(projectile_s *proj)
{
    partgrid_node_s *node = partgrid_node_pool_ffree;
    if (!node)
    {
        LOG_ERR("partgrid_node_pool full!");
        return NULL;
    }

    partgrid_node_pool_ffree = node->next_free;

    *node = (partgrid_node_s)
    {
        .projectile = proj,
    };

    return node;
}

static void partgrid_node_free(partgrid_node_s *node)
{
    node->projectile = NULL;
    node->next_free = partgrid_node_pool_ffree;
    partgrid_node_pool_ffree = node;
}

static partgrid_node_s* partgrid_cell_remove(partgrid_node_s **cell,
                                             const projectile_s *proj)
{
    if (!(*cell)) return NULL;

    if ((*cell)->projectile == proj)
    {
        partgrid_node_s *node = *cell;
        *cell = node->next;
        return node;
    }

    partgrid_node_s *node = *cell;
    partgrid_node_s *next = node->next;
    while (next)
    {
        if (next->projectile == proj)
        {
            node->next = next->next;
            return next;
        }

        node = next;
        next = next->next;
    }

    return NULL;
}

static void partgrid_cell_insert(partgrid_node_s **cell,
                                 partgrid_node_s *new_node)
{
    new_node->next = *cell;
    *cell = new_node;
}

void game_physics_move_projs(FIXED vel_mult)
{    
    projectile_s *projectile_end = g_game.projectiles + MAX_PROJECTILE_COUNT;
    proj_part_data_s *pdata = proj_data;
    for (projectile_s *proj = g_game.projectiles; proj != projectile_end;
        ++proj, ++pdata)
    {
        if (!IS_PROJ_ACTIVE(proj)) continue;

        proj->px += fxmul(proj->vx, vel_mult);
        proj->py += fxmul(proj->vy, vel_mult);

        int new_px = proj->px / (FIX_ONE * PARTGRID_CEL_W);
        int new_py = proj->py / (FIX_ONE * PARTGRID_CEL_H);

        // clamp position to partition grid
        if      (new_px < 0)              new_px = 0;
        else if (new_px >= PARTGRID_COLS) new_px = PARTGRID_COLS - 1;

        if      (new_py < 0)              new_py = 0;
        else if (new_py >= PARTGRID_ROWS) new_py = PARTGRID_ROWS - 1;
        
        // location in partition grid changed?
        part_cell_t *new_part_cell = &partgrid[new_py][new_px];
        if (pdata->part_cell != new_part_cell)
        {
            LOG_DBG("location in partition grid changed");

            // remove from linked list of old cell
            partgrid_node_s *node = pdata->part_cell
                ? partgrid_cell_remove(pdata->part_cell, proj)
                : NULL;
            if (!node)
            {
                LOG_DBG("old partition cell had no data");
                node = partgrid_node_alloc(proj);
            }

            // move node to new cell (or create a new node if not exists)
            partgrid_cell_insert(new_part_cell, node);
            pdata->part_cell = new_part_cell;
        }
    }
}











void game_physics_init(void)
{
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
        col_ent_map[i] = (entity_coldata_s){0};

    for (int i = 0; i < PARTGRID_NODE_POOL_SIZE - 1; ++i)
        partgrid_node_pool[i] = (partgrid_node_s)
        {
            .next_free = partgrid_node_pool + i + 1
        };

    partgrid_node_pool[PARTGRID_NODE_POOL_SIZE - 1] = (partgrid_node_s){0};
    partgrid_node_pool_ffree = partgrid_node_pool;

    for (int i = 0; i < PARTGRID_COLS * PARTGRID_ROWS; ++i)
        ((part_cell_t *)partgrid)[i] = NULL;

    memset32(x_contact_pairs, 0, ENTITY_PAIR_SIZE / 4);
    memset32(y_contact_pairs, 0, ENTITY_PAIR_SIZE / 4);
}

void game_physics_update(void)
{
    col_contact_count = 0;
    col_ent_count = 0;
    int substeps = 0;

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *entity = g_game.entities + i;

        bool is_col_ent = (entity->flags & ENTITY_FLAG_ENABLED) &&
                          (entity->flags & ENTITY_FLAG_COLLIDE);

        if (is_col_ent)
        {
            if (!col_ent_map[i].ent)
            {
                col_ent_map[i].ent = entity;
                col_ent_added(i);
            }
        }
        else
        {
            if (col_ent_map[i].ent)
            {
                col_ent_removed(i);
                col_ent_map[i].ent = NULL;
            }
        }

        if (!(entity->flags & ENTITY_FLAG_ENABLED))
            continue;
        
        if (entity->flags & ENTITY_FLAG_ACTOR)
            entity->actor.flags &= ~(ACTOR_FLAG_GROUNDED | ACTOR_FLAG_WALL |
                                    ACTOR_FLAG_DID_JUMP);
        
        if (!(entity->flags & ENTITY_FLAG_COLLIDE)) continue;
        
        col_ent_map[i].inv_mass = fxdiv(FIX_ONE * 2, int2fx((int)entity->mass));

        int speed = max(abs(entity->vel.x), abs(entity->vel.y));
        int subst = ceil_div(speed, FIX_ONE * 2);
        if (subst > substeps)
            substeps = subst;

        col_ents[col_ent_count++] = col_ent_map + i;
    }

    for (int i = 0; i < MAX_PROJECTILE_COUNT; ++i)
    {
        projectile_s *proj = g_game.projectiles + i;
        if (!IS_PROJ_ACTIVE(proj)) continue;

        int speed = max(abs(proj->vx), abs(proj->vy));
        int subst = ceil_div(speed, FIX_ONE * 2);
        if (subst > substeps)
            substeps = subst;
    }

    if (substeps > 8) substeps = 8;

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
        game_physics_move_projs(vel_mult);
        if (physics_substep(vel_mult))
        {
            for (int j = i; j < substeps; ++j)
                game_physics_move_projs(vel_mult);
            break;
        }
    }
}

void game_physics_on_proj_alloc(projectile_s *proj)
{
    intptr_t idx = proj - g_game.projectiles; // does this do division??
    LOG_DBG("projectile %i allocated", (int) idx);
    proj_data[idx] = (proj_part_data_s)
    {
        .part_cell = NULL
    };
}

void game_physics_on_proj_free(projectile_s *proj)
{
    intptr_t idx = proj - g_game.projectiles; // does this do division??
    LOG_DBG("projectile %i freed", (int) idx);

    partgrid_node_s *cell = partgrid_cell_remove(proj_data[idx].part_cell, proj);
    if (!cell)
    {
        LOG_WRN("game_physics_on_proj_free: projectile is not in partgrid?");
        return;
    }

    partgrid_node_free(cell);
}