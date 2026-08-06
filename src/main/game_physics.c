#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include <tonc_math.h>
#include <tonc_types.h>
#include <log.h>

#include "game.h"
#include "game_physics.h"
#include "datastruct.h"
#include "math_util.h"
#include <platutil.h>

//------------------------------------------------------------------------------
// declarations
//------------------------------------------------------------------------------
#pragma region declarations

// #define PHYS_PROFILE

// realistically, i doubt using the `static inline` fxmul and fxdiv in ARM code
// would create invocations of those functions as thumb code. but i mean. they
// can be turned into macros pretty easily with no drawbacks whatsoever. so why
// not I guess.
#define FXMUL(fa,fb) (((fa)*(fb))>>FIX_SHIFT)
#define FXDIV(fa,fb) (((fa)*FIX_SCALE)/(fb))

#ifdef PHYS_PROFILE
#define PROFILE_START() profile_start()
#define PROFILE_END(t) profile.t += profile_stop()
#define PROFILE_END2(t) profile.t = profile_stop()
#else
#define PROFILE_START()
#define PROFILE_END(t)
#define PROFILE_END2(t)
#endif

#ifdef PHYS_PROFILE
struct phys_profile
{
    u32 detection_ent_t;
    u32 detection_tile_t;
    u32 resolution_t;
    u32 move_t;
    u32 start_t;
    u32 projectiles_t;
} profile;

#define PROFILE_LOG(str, n)  \
    do  \
    {  \
        int v = profile.n * 100;  \
        LOG_DBG(str ": %i.%02i%%", v / 280896, v % 280896 / 2809);  \
    } while (false)

#define PROFILE_LOG_CYCLES(str, n)  \
    do  \
    {  \
        LOG_DBG(str ": %i cycles", profile.n);  \
    } while (false)
#endif

typedef struct aabb
{
    int left, top;
    int right, bottom;
}
aabb_s;

#pragma endregion declarations







//------------------------------------------------------------------------------
// entity physics 
//------------------------------------------------------------------------------
#pragma region entity physics

// in world units (8 units per tile)
#define ENT_PGRID_CELL_W 32
#define ENT_PGRID_CELL_H 32
#define ENT_PGRID_COLS  16
#define ENT_PGRID_ROWS  16
#define ENT_PGRID_NODE_POOL_SIZE 128

// collision-processing data for each active entity
typedef struct entity_coldata
{
    entity_s *ent;
    FIXED inv_mass;
    FIXED width, height;
    FIXED half_width, half_height;
    bool dirty;
    bool head_bump;
    s8 x_anchor;
    s8 y_anchor;
    aabb_s pgrid_bounds;
}
entity_coldata_s;

typedef struct col_contact
{
    FIXED nx, ny, pd;
    entity_coldata_s *ent_a, *ent_b;
    u8 priority;
    s16 tx, ty;
}
col_contact_s;

// narrow-phase collision detection. contains penetration vector.
typedef struct col_overlap_res {
    bool overlap;
    FIXED nx, ny, pd;
} col_overlap_res_s;

typedef struct ent_pgrid_node
{
    entity_coldata_s *cent;

    // if active (i.e. cent != NULL), this points to the next node
    // in the partition cell linked list. if inactive, this instead points to
    // the next unallocated node in the pool.
    union
    {
        struct ent_pgrid_node *next;
        struct ent_pgrid_node *next_free;
    };
}
ent_pgrid_node_s;

typedef ent_pgrid_node_s *ent_pgrid_cell_t;

static ent_pgrid_node_s ent_pgrid_node_pool[ENT_PGRID_NODE_POOL_SIZE];
static ent_pgrid_node_s *ent_pgrid_node_pool_ffree; // first free
static ent_pgrid_cell_t ent_pgrid[ENT_PGRID_ROWS][ENT_PGRID_COLS];

static int col_ent_count = 0;
static entity_coldata_s col_ent_map[MAX_ENTITY_COUNT];
static entity_coldata_s *col_ents[MAX_ENTITY_COUNT];

static uint col_contact_count = 0;
static col_contact_s col_contacts[MAX_CONTACT_COUNT];








// (w, h) = half-extents
static inline col_overlap_res_s rect_collision(FIXED x0, FIXED y0, FIXED hw0,
                                               FIXED hh0, FIXED x1, FIXED y1,
                                               FIXED hw1, FIXED hh1)
{
    col_overlap_res_s res;
    res.overlap = false;

    FIXED nx = (x1 + hw1) - (x0 + hw0);
    FIXED x_overlap = hw0 + hw1 - ABS(nx);
    if (x_overlap <= 0) goto ret;

    FIXED ny = (y1 + hh1) - (y0 + hh0);
    FIXED y_overlap = hh0 + hh1 - ABS(ny);
    if (y_overlap <= 0) goto ret;

    res.overlap = true;
    if (x_overlap < y_overlap)
    {
        res.nx = int2fx(SGN(nx));
        res.ny = 0;
        res.pd = x_overlap;
    }
    else
    {
        res.nx = 0;
        res.ny = int2fx(SGN(ny));
        res.pd = y_overlap;
    }

    ret:
        return res;
}

ARM_FUNC NO_INLINE
static ent_pgrid_node_s* ent_pgrid_node_alloc(entity_coldata_s *cent)
{
    ent_pgrid_node_s *node = ent_pgrid_node_pool_ffree;
    if (!node)
    {
        LOG_ERR("ent_pgrid_node_pool full!");
        return NULL;
    }

    ent_pgrid_node_pool_ffree = node->next_free;

    *node = (ent_pgrid_node_s)
    {
        .cent = cent,
    };

    return node;
}

ARM_FUNC NO_INLINE
static void ent_pgrid_node_free(ent_pgrid_node_s *node)
{
    if (!node) return;

    node->cent = NULL;
    node->next_free = ent_pgrid_node_pool_ffree;
    ent_pgrid_node_pool_ffree = node;
}

ARM_FUNC NO_INLINE
static void ent_pgrid_cell_insert(ent_pgrid_cell_t *cell,
                                  ent_pgrid_node_s *new_node)
{
    new_node->next = *cell;
    *cell = new_node;
}

ARM_FUNC NO_INLINE
static ent_pgrid_node_s* ent_pgrid_cell_remove(ent_pgrid_cell_t *cell,
                                               const entity_coldata_s *cent)
{
    if (!(*cell)) return NULL;

    if ((*cell)->cent == cent)
    {
        ent_pgrid_node_s *node = *cell;
        *cell = node->next;
        return node;
    }

    ent_pgrid_node_s *node = *cell;
    ent_pgrid_node_s *next = node->next;
    while (next)
    {
        if (next->cent == cent)
        {
            node->next = next->next;
            next->next = NULL;
            return next;
        }

        node = next;
        next = next->next;
    }

    return NULL;
}

ARM_FUNC NO_INLINE
static bool ent_pgrid_add(entity_coldata_s *cent, const aabb_s *aabb)
{
    const int left = aabb->left;
    const int top = aabb->top;
    const int right = aabb->right;
    const int bottom = aabb->bottom;

    // allocate all nodes upfront
    size_t node_count = (right - left + 1) * (bottom - top + 1);
#ifdef DEVDEBUG
    // something is seriously wrong...
    if (node_count >= 20) DBG_CRASH();
#endif
    ent_pgrid_node_s **nodes = alloca(node_count * sizeof(void*));

#ifdef PLATFORM_GBA
    memset32(nodes, 0, node_count);
#else
    memset(nodes, 0, node_count * sizeof(void*));
#endif

    size_t nodeidx = 0;
    for (; nodeidx < node_count; ++nodeidx)
    {
        ent_pgrid_node_s *node = ent_pgrid_node_alloc(cent);
        if (!node)
        {
            // allocation error. abort!
            for (size_t i = 0; i < nodeidx; ++i)
                ent_pgrid_node_free(nodes[i]);
            return false;
        }

        nodes[nodeidx] = node;
    }
    nodeidx = 0;

    // add each node to each cell
    for (uint y = top; y <= bottom; ++y)
    for (uint x = left; x <= right; ++x)
        ent_pgrid_cell_insert(&ent_pgrid[y][x], nodes[nodeidx++]);

    // NOLINTNEXTLINE(readability-misleading-indentation)
    return true;
}

ARM_FUNC NO_INLINE
static aabb_s calc_ent_pgrid_bounds(const entity_s *ent)
{
    int l = ent->pos.x / (FIX_ONE * ENT_PGRID_CELL_W);
    l = clamp(l, 0, ENT_PGRID_COLS);
    int t = ent->pos.y / (FIX_ONE * ENT_PGRID_CELL_H);
    t = clamp(t, 0, ENT_PGRID_ROWS);
    int r = (ent->pos.x + FX(ent->col.w)) / (FIX_ONE * ENT_PGRID_CELL_W);
    r = clamp(r, 0, ENT_PGRID_COLS);
    int b = (ent->pos.y + FX(ent->col.h)) / (FIX_ONE * ENT_PGRID_CELL_H);
    b = clamp(b, 0, ENT_PGRID_ROWS);

    return (aabb_s)
    {
        .left = l,
        .top = t,
        .right = r,
        .bottom = b
    };
}

ARM_FUNC NO_INLINE
static void ent_pgrid_remove(entity_coldata_s *cent, const aabb_s *aabb)
{
    const int left = aabb->left;
    const int top = aabb->top;
    const int right = aabb->right;
    const int bottom = aabb->bottom;

    for (uint y = top; y <= bottom; ++y)
    for (uint x = left; x <= right; ++x)
    {
        ent_pgrid_node_s *node = ent_pgrid_cell_remove(&ent_pgrid[y][x], cent);
        ent_pgrid_node_free(node);
    }
}

static inline bool aabb_equal(const aabb_s *a, const aabb_s *b)
{
    return a->left == b->left && a->right == b->right &&
           a->bottom == b->bottom && a->top == b->top;
}

static void col_ent_added(int col_ent_idx)
{
    LOG_DBG("col_ent_added %i", col_ent_idx);
    entity_coldata_s *cent = col_ent_map + col_ent_idx;

    aabb_s aabb = calc_ent_pgrid_bounds(cent->ent);
    cent->pgrid_bounds = aabb;
    ent_pgrid_add(cent, &aabb);
}

static void col_ent_removed(int col_ent_idx)
{
    LOG_DBG("col_ent_removed");

    entity_coldata_s *cent = col_ent_map + col_ent_idx;

    aabb_s aabb = calc_ent_pgrid_bounds(cent->ent);
    ent_pgrid_remove(cent, &aabb);
}

// a body is anchored if:
// 1. it is static (i.e. does not have ENTITY_FLAG_MOVING set)
// 2. it has previously collided with an anchored object in the same
//    direction.
// this function only checks for condition #2.
static inline bool is_body_anchored(entity_coldata_s *col_ent, FIXED nx,
                                    FIXED ny)
{
    if (col_ent->x_anchor != 0 && (int)col_ent->x_anchor == sgn3(nx)) return true;
    if (col_ent->y_anchor != 0 && (int)col_ent->y_anchor == sgn3(ny)) return true;
    return false;
}

ARM_FUNC
static bool physics_substep_process_contact(entity_coldata_s *col_ent,
                                            entity_coldata_s *entc2)
{
    // make sure that the second entity is the static one
    if (entc2->ent->flags & ENTITY_FLAG_MOVING)
    {
        entity_coldata_s *temp;
        SWAP3(col_ent, entc2, temp);
    }

    entity_s *entity = col_ent->ent;
    entity_s *ent2 = entc2->ent;

    // don't handle collision if one of the entities is queued to be
    // freed
    if ((entity->flags & ENTITY_FLAG_QFREE) ||
        (ent2->flags & ENTITY_FLAG_QFREE))
    {
        return true;
    }

    // if both entities are static objects, collision cannot happen
    // between them.
    if (!(entity->flags & ENTITY_FLAG_MOVING) &&
        !(ent2->flags & ENTITY_FLAG_MOVING))
    {
        return true;
    }

    if (col_ent == entc2)
    {
        LOG_WRN("entity contact is with itself? how tf?");
        return true;
    }

    // narrow-phase collision. also calculates penetration vector.
    const FIXED hw0 = col_ent->half_width;
    const FIXED hh0 = col_ent->half_height;
    const FIXED hw1 = entc2->half_width;
    const FIXED hh1 = entc2->half_height;
    
    col_overlap_res_s overlap_res =
        rect_collision(entity->pos.x, entity->pos.y, hw0, hh0,
                        ent2->pos.x, ent2->pos.y, hw1, hh1);

    if (!overlap_res.overlap)
        return true;
    if (overlap_res.ny <= 0 && ent2->col.flags & COL_FLAG_FLOOR_ONLY)
        return true;
    if (overlap_res.ny >= 0 && entity->col.flags & COL_FLAG_FLOOR_ONLY)
        return true;

    // add contact to contact list
    if (col_contact_count >= MAX_CONTACT_COUNT)
    {
        LOG_WRN("max contacts exceeded!");
        return false;
    }

    col_contacts[col_contact_count] = (col_contact_s)
    {
        .nx = overlap_res.nx,
        .ny = overlap_res.ny,
        .pd = overlap_res.pd,
        .ent_a = col_ent,
        .ent_b = entc2,
        .priority = (entity->flags & ENTITY_FLAG_MOVING) ||
                    (ent2->flags & ENTITY_FLAG_MOVING)
    };
    ++col_contact_count;

    // run behavior callbacks
    int nx_int = sgn3(overlap_res.nx);
    int ny_int = sgn3(overlap_res.ny);

    if (entity->behavior && entity->behavior->ent_touch)
        entity->behavior->ent_touch(entity, ent2, nx_int, ny_int);
    if (ent2->behavior && ent2->behavior->ent_touch)
        ent2->behavior->ent_touch(ent2, entity, -nx_int, -ny_int);

    return true;
}

ARM_FUNC NO_INLINE
static void physics_substep_collect_contacts(void)
{
    PROFILE_START();

    // not a very useful optimization, compared to just checking the dirty
    // (and perhaps the MOVING flag) for each colent, but whatever.
    size_t dirty_ent_count = 0;
    static entity_coldata_s *dirty_ents[MAX_ENTITY_COUNT];

    // update spatial partition
    for (uint i = 0; i < col_ent_count; ++i)
    {
        entity_coldata_s *const col_ent = col_ents[i];
        entity_s *entity = col_ent->ent;
        
        if (!col_ent->dirty)
            continue;

        aabb_s new_aabb = calc_ent_pgrid_bounds(entity);
        if (!aabb_equal(&col_ent->pgrid_bounds, &new_aabb))
        {
            ent_pgrid_remove(col_ent, &col_ent->pgrid_bounds);
            ent_pgrid_add(col_ent, &new_aabb);
            col_ent->pgrid_bounds = new_aabb;
        }

        if (!(entity->flags & ENTITY_FLAG_MOVING))
        {
            col_ent->dirty = false;
            continue;
        }

        dirty_ents[dirty_ent_count++] = col_ent;
    }

    col_contact_count = 0;

    // collect entity contacts
    for (uint i = 0; i < dirty_ent_count; ++i)
    {
        entity_coldata_s *col_ent = dirty_ents[i];
        // if (!col_ent->dirty)
        //     continue;
        // if (!(col_ent->ent->flags & ENTITY_FLAG_MOVING))
        //     continue;

        aabb_s pgrid_bounds = col_ent->pgrid_bounds;
        for (uint y = pgrid_bounds.top; y <= pgrid_bounds.bottom; ++y)
        for (uint x = pgrid_bounds.left; x <= pgrid_bounds.right; ++x)
        {
            for (ent_pgrid_node_s *node = ent_pgrid[y][x]; node;
                 node = node->next)
            {
                if (col_ent == node->cent)
                    continue;

                bool s = physics_substep_process_contact(col_ent, node->cent);
                if (!s)
                {
                    PROFILE_END(detection_ent_t);
                    goto end;
                }
            }
            
        }
    }

    PROFILE_END(detection_ent_t);

    // collect tile contacts
    PROFILE_START();

    for (uint i = 0; i < dirty_ent_count; ++i)
    {
        if (col_contact_count >= MAX_CONTACT_COUNT)
        {
            LOG_WRN("max contacts exceeded!");
            PROFILE_END(detection_tile_t);
            goto end;
        }

        entity_coldata_s *const col_ent = dirty_ents[i];
        entity_s *entity = col_ent->ent;

        if (!(entity->flags & ENTITY_FLAG_MOVING)) continue;

        // only calculate tile overlaps if this entity moved in the previous
        // iteration. (or, if it's the first iteration.)
        // if (!col_ent->dirty) continue;
        col_ent->dirty = false;

        const FIXED col_w = col_ent->width;
        const FIXED col_h = col_ent->height;
        const FIXED col_half_w = col_ent->half_width;
        const FIXED col_half_h = col_ent->half_height;

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

            // bool tile_col_found = false;
            // FIXED final_nx = 0, final_ny = 0, final_pd = 0;
            // int tx, ty;
            for (int y = min_y; y <= max_y; ++y)
            {
                for (int x = min_x; x <= max_x; ++x)
                {
                    if (game_get_col_clamped(x, y) != 1)
                        continue;

                    const FIXED tx = x * FIX_ONE * WORLD_TILE_SIZE;
                    const FIXED ty = y * FIX_ONE * WORLD_TILE_SIZE;

                    col_overlap_res_s overlap_res = 
                        rect_collision(entity->pos.x, entity->pos.y,
                                        col_half_w, col_half_h, tx, ty,
                                        int2fx(WORLD_TILE_SIZE) / 2,
                                        int2fx(WORLD_TILE_SIZE) / 2);
                    
                    if (!overlap_res.overlap) continue;

                    col_contacts[col_contact_count] = (col_contact_s)
                    {
                        .nx = overlap_res.nx,
                        .ny = overlap_res.ny,
                        .pd = overlap_res.pd,
                        .ent_a = col_ent,
                        .ent_b = NULL,
                        .priority = 1,
                        .tx = tx,
                        .ty = ty,
                    };
                    ++col_contact_count;
                }
            }
        }
    }

    PROFILE_END(detection_tile_t);

    end:;
    // sort contacts such that those with the most penetration will be
    // processed first. also, i want contacts containing static bodies to
    // be processed first as well.
    // TODO: is it faster to use a heap priority queue?
    // i hypothesize it will either not be or the difference will be
    // negligible, since this insertion sort runs pretty quickly already. i
    // think.
    for (int i = 1; i < col_contact_count; ++i)
    {
        for (int j = i - 1; j >= 0; --j)
        {
            col_contact_s *const c0 = col_contacts + j;
            col_contact_s *const c1 = col_contacts + j + 1;

            if (c1->pd > c0->pd || c1->priority > c0->priority)
            {
                col_contact_s tmp;
                SWAP3(col_contacts[j], col_contacts[j+1], tmp);
            }
        }
    }
}

// turn this into an ARM+IWRAM function to save 0-4% of the CPU!
static bool physics_substep_resolve_contacts(void)
{
    bool break_substep = true;

    // resolve collected contacts
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

        if ((ent_a->col.flags & COL_FLAG_MONITOR_ONLY) ||
            (ent_b && ent_b->col.flags & COL_FLAG_MONITOR_ONLY))
        {
            continue;
        }

        // if one of the entities already moved before in the same
        // iteration, then recalculate the penetration vector. if there is
        // no more overlap, just skip the contact.
        if (col_ent_a->dirty && !col_ent_b)
        {
            const FIXED tx = contact->tx;
            const FIXED ty = contact->ty;

            col_overlap_res_s test_overlap =
                rect_collision(ent_a->pos.x, ent_a->pos.y,
                                col_ent_a->half_width, col_ent_a->half_height,
                                tx, ty,
                                int2fx(WORLD_TILE_SIZE) / 2,
                                int2fx(WORLD_TILE_SIZE) / 2);
            
            if (!test_overlap.overlap) continue;
            nx = test_overlap.nx;
            ny = test_overlap.ny;
            pd = test_overlap.pd;
        }
        else if (col_ent_b && (col_ent_a->dirty || col_ent_b->dirty))
        {
            col_overlap_res_s test_overlap =
                rect_collision(ent_a->pos.x, ent_a->pos.y,
                                col_ent_a->half_width,
                                col_ent_a->half_height,
                                ent_b->pos.x, ent_b->pos.y,
                                col_ent_b->half_width,
                                col_ent_b->half_height);
            
            if (!test_overlap.overlap) continue;
            nx = test_overlap.nx;
            ny = test_overlap.ny;
            pd = test_overlap.pd;
        }

        // calculate relative velocity of impact
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

        // don't process if the objects are moving away from each other.
        FIXED vdot = FXMUL(nx, rel_vx) + FXMUL(ny, rel_vy);
        
        if (vdot < 0) continue;

        uint col_group_a = (uint)ent_a->col.group;
        uint col_mask_a = (uint)ent_a->col.mask;

        // check collision groups
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

        // check if this is a collision with an anchored body.
        // a body is anchored if:
        // 1. it is static (i.e. does not have ENTITY_FLAG_MOVING set)
        // 2. it has previously collided with an anchored object in the same
        //    direction.
        bool anchor_a = false;
        bool anchor_b = true;
        if (col_ent_b)
        {
            anchor_a = is_body_anchored(col_ent_a, nx, ny);
            anchor_b = !(ent_b->flags & ENTITY_FLAG_MOVING) ||
                        is_body_anchored(col_ent_b, -nx, -ny);
        }
        
        if (anchor_a || anchor_b)
        {
            // LOG_DBG("%i: anchor collision", subsubstep);
            // if (anchor_a || ent_b) LOG_DBG("Anchors away!!");

            entity_s *ent;
            entity_coldata_s *ce;

            // we only want to modify the entity that is not anchored.
            if (anchor_b)
            {
                ent = ent_a;
                ce = col_ent_a;
            }
            else
            {
                ent = ent_b;
                ce = col_ent_b;
                nx = -nx;
                ny = -ny;
            }

            // mark that the entity moved. tile contacts will be
            // recalculated for this entity on the next iteration.
            ce->dirty = true;

            FIXED px = FXMUL(nx, pd);
            FIXED py = FXMUL(ny, pd);

            ent->pos.x = ent->pos.x - px;
            ent->pos.y = ent->pos.y - py;
            ent->vel.x = ent->vel.x - FXMUL(nx, vdot);
            ent->vel.y = ent->vel.y - FXMUL(ny, vdot);

            if (nx != 0) ce->x_anchor = sgn(-nx);
            if (ny != 0) ce->y_anchor = sgn(-ny);

            if (ny > 0)
                ent->actor.flags |= ACTOR_FLAG_GROUNDED;

            if (nx != 0)
                ent->actor.flags |= ACTOR_FLAG_WALL;
        }
        else
        {
            // LOG_DBG("%i: free collision", subsubstep);
            
            // mark that these entity moved. tile contacts will be
            // recalculated for this entity on the next iteration.
            col_ent_a->dirty = true;
            col_ent_b->dirty = true;

            FIXED inv_mass1 = col_ent_a->inv_mass;
            FIXED inv_mass2 = col_ent_b->inv_mass;

            FIXED total_inv_mass = inv_mass1 + inv_mass2;
            if (total_inv_mass == 0) continue;
            FIXED inv_total_inv_mass = FXDIV(FIX_ONE, total_inv_mass);

            // i want head bumps to move the bodies more. i also want it
            // to work in a chain. this is a kind of hacky solution for
            // parity with the physics of the original unyuland, since a
            // puzzle depends on a head-bump chain moving the last entity
            // far up enough to go over a one-block step.
            FIXED restitution;
            if (ny != 0 && (col_ent_a->head_bump || col_ent_b->head_bump))
            {
                restitution = TO_FIXED(1.9);
                col_ent_a->head_bump = true;
                col_ent_b->head_bump = true;
            }
            else
            {
                restitution = TO_FIXED(1.2);
            }
            
            FIXED impulse_fac =
                FXMUL(FXMUL(restitution, vdot), inv_total_inv_mass);
            FIXED impulse_x = FXMUL(nx, impulse_fac);
            FIXED impulse_y = FXMUL(ny, impulse_fac);

            ent_a->vel.x -= FXMUL(impulse_x, inv_mass1);
            ent_a->vel.y -= FXMUL(impulse_y, inv_mass1);
            ent_b->vel.x += FXMUL(impulse_x, inv_mass2);
            ent_b->vel.y += FXMUL(impulse_y, inv_mass2);

            // don't perform regular multiplication here. instead, do a
            // version that rounds up the product. because if the
            // penetration depth is too small, the product can end up
            // as zero due to rounding, causing no movement to be done and
            // thus the collision will never resolve.
            FIXED move_x = ceil_div(FXMUL(nx, pd) * inv_total_inv_mass, FIX_ONE);
            FIXED move_y = ceil_div(FXMUL(ny, pd) * inv_total_inv_mass, FIX_ONE);

            ent_a->pos.x -= ceil_div(move_x * inv_mass1, FIX_ONE);
            ent_a->pos.y -= ceil_div(move_y * inv_mass1, FIX_ONE);
            ent_b->pos.x += ceil_div(move_x * inv_mass2, FIX_ONE);
            ent_b->pos.y += ceil_div(move_y * inv_mass2, FIX_ONE);

            if (ny > 0)
                ent_a->actor.flags |= ACTOR_FLAG_GROUNDED;
            else if (ny < 0)
                ent_b->actor.flags |= ACTOR_FLAG_GROUNDED;

            if (nx != 0)
            {
                ent_a->actor.flags |= ACTOR_FLAG_WALL;
                ent_b->actor.flags |= ACTOR_FLAG_WALL;
            }
        }

        break_substep = false;
    }

    return break_substep;
}

static bool physics_substep(FIXED vel_mult)
{
    PROFILE_START();

    // first move all entities. also reset substep-local state.
    for (int i = 0; i < col_ent_count; ++i)
    {
        entity_coldata_s *const col_ent = col_ents[i];
        entity_s *entity = col_ent->ent;

        if (entity->flags & ENTITY_FLAG_MOVING)
        {
            FIXED s_vx = FXMUL(entity->vel.x, vel_mult);
            FIXED s_vy = FXMUL(entity->vel.y, vel_mult);
            entity->pos.x += s_vx;
            entity->pos.y += s_vy;

            col_ent->dirty = true;
        }

        col_ent->head_bump = entity->col.flags & COL_FLAG_HEAD_BUMP;
        col_ent->x_anchor = 0;
        col_ent->y_anchor = 0;
    }

    PROFILE_END(move_t);
    
    // perform iterations. more iterations are needed the more complex the
    // system is wrt collisions with multiple entities.
    int subsubstep = 1;
    for (;; ++subsubstep)
    {
        if (subsubstep >= 6)
        {
            LOG_WRN("exceeded max iterations");
            break;
        }

        physics_substep_collect_contacts();

        PROFILE_START();
        bool break_substep;
        break_substep = physics_substep_resolve_contacts();
        PROFILE_END(resolution_t);

        // LOG_DBG("iteration %i: %i", subsubstep, resolved_contacts);
        if (break_substep) break;
    }

    #ifdef PHYS_PROFILE
    LOG_DBG("ITERATION COUNT: %i", subsubstep);
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

#pragma endregion entity physics







//------------------------------------------------------------------------------
// projectile physics
//------------------------------------------------------------------------------
#pragma region projectile physics

// in world units (8 units per tile)
#define PROJ_PGRID_CELL_W 64
#define PROJ_PGRID_CELL_H 64
#define PROJ_PGRID_COLS  8
#define PROJ_PGRID_ROWS  8
#define PROJ_PGRID_NODE_POOL_SIZE 128

typedef struct proj_pgrid_node
{
    projectile_s *projectile;

    // if active (i.e. projectile != NULL), this points to the next node
    // in the partition cell linked list. if inactive, this instead points to
    // the next unallocated node in the pool.
    union
    {
        struct proj_pgrid_node *next;
        struct proj_pgrid_node *next_free;
    };
} proj_pgrid_node_s;

typedef proj_pgrid_node_s *proj_pgrid_cell_t;

typedef struct proj_pgrid_data
{
    proj_pgrid_cell_t *part_cell;
}
proj_pgrid_data_s;

static proj_pgrid_data_s proj_data[MAX_PROJECTILE_COUNT];
static proj_pgrid_node_s proj_pgrid_node_pool[PROJ_PGRID_NODE_POOL_SIZE];
static proj_pgrid_node_s *proj_pgrid_node_pool_ffree; // first free
static proj_pgrid_cell_t proj_pgrid[PROJ_PGRID_ROWS][PROJ_PGRID_COLS];







static proj_pgrid_node_s* proj_pgrid_node_alloc(projectile_s *proj)
{
    proj_pgrid_node_s *node = proj_pgrid_node_pool_ffree;
    if (!node)
    {
        LOG_ERR("proj_pgrid_node_pool full!");
        return NULL;
    }

    proj_pgrid_node_pool_ffree = node->next_free;

    *node = (proj_pgrid_node_s)
    {
        .projectile = proj,
    };

    return node;
}

static void proj_pgrid_node_free(proj_pgrid_node_s *node)
{
    node->projectile = NULL;
    node->next_free = proj_pgrid_node_pool_ffree;
    proj_pgrid_node_pool_ffree = node;
}

static proj_pgrid_node_s* proj_pgrid_cell_remove(proj_pgrid_node_s **cell,
                                                 const projectile_s *proj)
{
    if (!(*cell)) return NULL;

    if ((*cell)->projectile == proj)
    {
        proj_pgrid_node_s *node = *cell;
        *cell = node->next;
        return node;
    }

    proj_pgrid_node_s *node = *cell;
    proj_pgrid_node_s *next = node->next;
    while (next)
    {
        if (next->projectile == proj)
        {
            node->next = next->next;
            next->next = NULL;
            return next;
        }

        node = next;
        next = next->next;
    }

    return NULL;
}

static void proj_pgrid_cell_insert(proj_pgrid_node_s **cell,
                                 proj_pgrid_node_s *new_node)
{
    new_node->next = *cell;
    *cell = new_node;
}

ARM_FUNC NO_INLINE
static void game_physics_move_projs(FIXED vel_mult)
{
    for (uint i = 0; i < MAX_PROJECTILE_COUNT; ++i)
    {
        projectile_s *proj = g_game.projectiles + i;
        proj_pgrid_data_s *pdata = proj_data + i;
        if (!IS_PROJ_ACTIVE(proj) || (proj->flags & PROJ_FLAG_QFREE)) continue;

        proj->px += FXMUL(proj->vx, vel_mult);
        proj->py += FXMUL(proj->vy, vel_mult);

        // if projectile touches a wall, Destroy it.
        int tx = proj->px / (WORLD_TILE_SIZE * FIX_SCALE);
        int ty = proj->py / (WORLD_TILE_SIZE * FIX_SCALE);
        if (tx < 0) --tx;
        if (ty < 0) --ty;

        int map_col = game_get_col_clamped(tx, ty);
        if (map_col == 1 || map_col == 2)
        {
            // it was Destroyed.
            projectile_queue_free(proj);
            continue;
        }

        int new_px = proj->px / (FIX_ONE * PROJ_PGRID_CELL_W);
        int new_py = proj->py / (FIX_ONE * PROJ_PGRID_CELL_H);

        // clamp position to partition grid
        if      (new_px < 0)                new_px = 0;
        else if (new_px >= PROJ_PGRID_COLS) new_px = PROJ_PGRID_COLS - 1;

        if      (new_py < 0)                new_py = 0;
        else if (new_py >= PROJ_PGRID_ROWS) new_py = PROJ_PGRID_ROWS - 1;
        
        // location in partition grid changed?
        proj_pgrid_cell_t *new_part_cell = &proj_pgrid[new_py][new_px];
        if (pdata->part_cell != new_part_cell)
        {
            // LOG_DBG("location in partition grid changed");

            // remove from linked list of old cell
            proj_pgrid_node_s *node = pdata->part_cell
                ? proj_pgrid_cell_remove(pdata->part_cell, proj)
                : NULL;
            if (!node)
            {
                // LOG_DBG("old partition cell had no data");
                node = proj_pgrid_node_alloc(proj);
                if (!node) DBG_CRASH();
            }

            // move node to new cell (or create a new node if not exists)
            proj_pgrid_cell_insert(new_part_cell, node);
            pdata->part_cell = new_part_cell;
        }
    }

    // projectile/entity collision detection
    for (int i = 0; i < col_ent_count; ++i)
    {
        entity_coldata_s *const col_ent = col_ents[i];
        entity_s *entity = col_ent->ent;

        if (!(entity->col.mask & COLGROUP_PROJECTILE)) continue;
        if (entity->col.flags & COL_FLAG_MONITOR_ONLY) continue;

        const FIXED col_w = col_ent->width;
        const FIXED col_h = col_ent->height;

        const int el = entity->pos.x;
        const int et = entity->pos.y;
        const int er = (entity->pos.x + col_w);
        const int eb = (entity->pos.y + col_h);

        int min_px = el / (FIX_ONE * PROJ_PGRID_CELL_W);
        int min_py = et / (FIX_ONE * PROJ_PGRID_CELL_H);
        int max_px = er / (FIX_ONE * PROJ_PGRID_CELL_W);
        int max_py = eb / (FIX_ONE * PROJ_PGRID_CELL_H);

        // clamp bounds to partition grid
        if      (min_px < 0)                min_px = 0;
        else if (min_px >= PROJ_PGRID_COLS) min_px = PROJ_PGRID_COLS - 1;
        if      (max_px < 0)                max_px = 0;
        else if (max_px >= PROJ_PGRID_COLS) max_px = PROJ_PGRID_COLS - 1;

        if      (min_py < 0)                min_py = 0;
        else if (min_py >= PROJ_PGRID_ROWS) min_py = PROJ_PGRID_ROWS - 1;
        if      (max_py < 0)                max_py = 0;
        else if (max_py >= PROJ_PGRID_ROWS) max_py = PROJ_PGRID_ROWS - 1;

        for (int y = min_py; y <= max_py; ++y)
        {
            for (int x = min_px; x <= max_px; ++x)
            {
                // traverse linked list
                for (proj_pgrid_node_s *node = proj_pgrid[y][x]; node;
                     node = node->next)
                {
                    projectile_s *proj = node->projectile;

                    if (proj->flags & PROJ_FLAG_QFREE) continue;

                    FIXED px = proj->px;
                    FIXED py = proj->py;

                    if (!(px > el && px < er && py > et && py < eb))
                        continue;

                    if (proj->did_touch_this_frame) continue;
                    proj->did_touch_this_frame = true;

                    // if (!(entity->flags & ENTITY_FLAG_MOVING))
                    //     LOG_DBG("Proc");
                    
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
}

#pragma endregion projectile physics







//------------------------------------------------------------------------------
// public
//------------------------------------------------------------------------------
#pragma region public

// turn this into an ARM+IWRAM function to save 1-6% of the CPU!
static void physics_prologue(FIXED *p_vmult, int *p_substeps)
{
    col_contact_count = 0;
    col_ent_count = 0;
    int substeps = 0;

    // preprocessing pass:
    //  - add/remove entities to collision data structures
    //    (i.e. sweep and prune)
    //  - reset physics state flags
    //  - calculate substeps needed for simulation based on fastest-moving
    //    entity
    //  - cache inverse mass and half-extents (although caching size-related data
    //    is probably pointless for performance...)
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
            entity->actor.flags &= ~(ACTOR_FLAG_GROUNDED | ACTOR_FLAG_WALL);
        
        if (!(entity->flags & ENTITY_FLAG_COLLIDE)) continue;
        
        entity_coldata_s *col_ent = &col_ent_map[i];
        col_ent->inv_mass = FXDIV(FIX_ONE * 2, int2fx((int)entity->mass));
        col_ent->width = int2fx((int) entity->col.w);
        col_ent->height = int2fx((int) entity->col.h);
        col_ent->half_width = col_ent->width / 2;
        col_ent->half_height = col_ent->height / 2;

        int speed = max(abs(entity->vel.x), abs(entity->vel.y));
        int subst = ceil_div(speed, FIX_ONE * 4);
        if (subst > substeps)
            substeps = subst;

        col_ents[col_ent_count++] = col_ent;
    }

    // find fastest-moving projectile, use for determining how many substeps
    // the simulation will need
    for (int i = 0; i < MAX_PROJECTILE_COUNT; ++i)
    {
        projectile_s *proj = g_game.projectiles + i;
        if (!IS_PROJ_ACTIVE(proj)) continue;

        int speed = max(abs(proj->vx), abs(proj->vy));
        int subst = ceil_div(speed, FIX_ONE * 4);
        if (subst > substeps)
            substeps = subst;
    }

    // cap substeps to 8. but entities usually don't move very fast, so
    // typically it will be 1 or 2.
    if (substeps > 8) substeps = 8;

    // round the substep count up to the nearest power of two. this is to make
    // division exact.
    --substeps;
    substeps |= substeps >> 1;
    substeps |= substeps >> 2;
    substeps |= substeps >> 4;
    substeps |= substeps >> 8;
    substeps |= substeps >> 16;
    ++substeps;

    FIXED vel_mult = 0;
    if (substeps != 0)
        vel_mult = FIX_ONE / substeps;

    *p_vmult = vel_mult;
    *p_substeps = substeps;
}

void game_physics_init(void)
{
    col_ent_count = 0;
    col_contact_count = 0;
    
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
        col_ent_map[i] = (entity_coldata_s){0};

    // initialize entity partition-grid
    for (int i = 0; i < ENT_PGRID_NODE_POOL_SIZE - 1; ++i)
        ent_pgrid_node_pool[i] = (ent_pgrid_node_s)
        {
            .next_free = ent_pgrid_node_pool + i + 1
        };

    ent_pgrid_node_pool[ENT_PGRID_NODE_POOL_SIZE - 1] = (ent_pgrid_node_s){0};
    ent_pgrid_node_pool_ffree = ent_pgrid_node_pool;

    for (int i = 0; i < ENT_PGRID_COLS * ENT_PGRID_ROWS; ++i)
        ((ent_pgrid_cell_t *)ent_pgrid)[i] = NULL;

    // initialize projectile partition-grid
    for (int i = 0; i < PROJ_PGRID_NODE_POOL_SIZE - 1; ++i)
        proj_pgrid_node_pool[i] = (proj_pgrid_node_s)
        {
            .next_free = proj_pgrid_node_pool + i + 1
        };

    proj_pgrid_node_pool[PROJ_PGRID_NODE_POOL_SIZE - 1] = (proj_pgrid_node_s){0};
    proj_pgrid_node_pool_ffree = proj_pgrid_node_pool;

    for (int i = 0; i < PROJ_PGRID_COLS * PROJ_PGRID_ROWS; ++i)
        ((proj_pgrid_cell_t *)proj_pgrid)[i] = NULL;
}

void game_physics_on_entity_alloc(entity_s *ent) {}
void game_physics_on_entity_free(entity_s *ent)
{
    uintptr_t i = ent - g_game.entities;
    entity_coldata_s *col = col_ent_map + i;

    if (!col->ent) return;
    if (col->ent != ent) DBG_CRASH();
    
    col_ent_removed(i);
    col->ent = NULL;
}

void game_physics_update(void)
{    
    #ifdef PHYS_PROFILE
    profile = (struct phys_profile){0};
    #endif

    FIXED vel_mult;
    int substeps;

    PROFILE_START();
    physics_prologue(&vel_mult, &substeps);
    PROFILE_END(start_t);

    PROFILE_START();

    {
        projectile_s *proj = g_game.projectiles;
        uint i = 0;
        for (; i < MAX_PROJECTILE_COUNT; ++i, ++proj)
            proj->did_touch_this_frame = false;
    }

    for (int iter = 0; iter < 2; ++iter)
        game_physics_move_projs(FIX_ONE / 2);

    PROFILE_END(projectiles_t);

    for (int i = 0; i < substeps; ++i)
    {
        if (physics_substep(vel_mult))
            break;
    }

    #ifdef PHYS_PROFILE
    PROFILE_LOG("e detection time", detection_ent_t);
    PROFILE_LOG("t detection time", detection_tile_t);
    PROFILE_LOG("resolution time", resolution_t);
    PROFILE_LOG("start time", start_t);
    // PROFILE_LOG("ent move time", move_t)
    // PROFILE_LOG("proj move time", projectiles_t)
    #endif
}

void game_physics_on_proj_alloc(projectile_s *proj)
{
    intptr_t idx = proj - g_game.projectiles; // does this do division??
    proj_data[idx] = (proj_pgrid_data_s)
    {
        .part_cell = NULL
    };
}

void game_physics_on_proj_free(projectile_s *proj)
{
    intptr_t idx = proj - g_game.projectiles; // does this do division??
    proj_pgrid_data_s *data = proj_data + idx;

    if (data->part_cell)
    {
        proj_pgrid_node_s *cell = proj_pgrid_cell_remove(data->part_cell, proj);
        *data = (proj_pgrid_data_s){0};

        if (!cell)
        {
            LOG_WRN("game_physics_on_proj_free: projectile is not in proj_pgrid?");
            return;
        }

        proj_pgrid_node_free(cell);
    }
}

#pragma endregion public