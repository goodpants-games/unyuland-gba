#include <tonc_math.h>
#include <tonc_input.h>
#include <tonc_video.h>
#include "game.h"
#include "log.h"
#include "math_util.h"
#include "sound.h"
#include "printf.h"
#include "gfx.h"

#define DEFAULT_DAMP TO_FIXED(0.88)

#define PLAYER_SIDE_SPIT_VX           TO_FIXED(0.50)
#define PLAYER_SIDE_SPIT_CLOSE_VX     TO_FIXED(0.25)
#define PLAYER_SIDE_SPIT_VY           TO_FIXED(3.00)
#define PLAYER_SIDE_SPIT_TARGET_Y_OFF TO_FIXED(5.00)
#define PLAYER_UP_SPIT_VY             TO_FIXED(3.00)
#define PLAYER_SPIT_G_MULT            TO_FIXED(2.00)

static bool droplet_check_tile(FIXED x, FIXED y)
{
    int tx = x / (WORLD_TILE_SIZE * FIX_ONE);
    int ty = y / (WORLD_TILE_SIZE * FIX_ONE);
    if (tx < 0) --tx;
    if (ty < 0) --ty;

    int col = game_get_col_clamped(tx, ty);
    if (col == 2)
        col = game_get_col_clamped(tx, ty - 1);

    return col == 0 || col == 2;
}










////////////////////
// GENERIC: enemy //
////////////////////
#pragma region G_enemy

typedef struct enemy_base
{
    s8 health;
}
enemy_base_s;

// if true, the calling function should return
static bool enemy_base_update(entity_s *self)
{
    enemy_base_s *data = (enemy_base_s *)&self->userdata;

    if (data->health < 0)
    {
        if (--data->health < -60)
            entity_queue_free(self);
        return true;
    }

    return false;
}

static void enemy_base_ent_touch(entity_s *self, entity_s *other, int nx, int ny)
{
    if (other == g_game.entities && other->behavior &&
        other->behavior->attacked)
    {
        int dx = sgn(other->pos.x - self->pos.x);
        other->behavior->attacked(other, self, dx);
    }
}

static bool enemy_base_proj_touch(entity_s *self, projectile_s *proj,
                                  sprid_game_e dead_gfx)
{
    enemy_base_s *data = (enemy_base_s *)&self->userdata;
    if (proj->kind != PROJ_KIND_PLAYER) return true;

    if (--data->health == 0)
    {
        data->health = -1;
        self->flags &= ~(ENTITY_FLAG_ACTOR | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_DAMPING);
        self->flags |= ENTITY_FLAG_MOVING;
        self->sprite.graphic_id = dead_gfx;
        self->sprite.flags &= ~SPRITE_FLAG_PLAYING;
        self->sprite.frame = 0;
        self->sprite.accum = 0;
        self->col.group = 0;
        self->col.mask = 0;

        // note: the original unyuland has a bug where the kinematics of fallen
        // enemies are ticked twice, so the numbers need to be adjusted here
        // since that bug is not present here.
        self->vel.x = TO_FIXED(0.5) * sgn(proj->vx);
        self->vel.y = TO_FIXED(-1.5);
        self->gmult = TO_FIXED(1.2);

        snd_play(SND_ID_ENEMY_DIE);
    }
    else
    {
        self->vel.x = 0;
        self->vel.y = 0;
    }

    return false;
}

#pragma endregion G_enemy








/////////////////////
// player_behavior //
/////////////////////
#pragma region player

const behavior_def_s behavior_player;

typedef struct player_data
{
    entity_s *interactable;
    entity_s *cursor;
    bool spitting;
    u8 cursor_frame;
    s8 death_timer;
} player_data_s;

void entity_player_init(entity_s *self)
{
    player_data_s *data = (player_data_s *)self->userdata;

    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR
                   | ENTITY_FLAG_KEEP_ON_ROOM_CHANGE;
    self->col.w = 6;
    self->col.h = 8;
    self->col.group = COLGROUP_ENTITY;
    self->col.mask = COLGROUP_DEFAULT | COLGROUP_PROJECTILE;
    self->actor.move_speed = TO_FIXED(1.0);
    self->actor.move_accel = TO_FIXED(1.0 / 8.0);
    self->actor.jump_velocity = TO_FIXED(2.0);
    self->sprite.ox = -1;
    self->sprite.oy = -8;
    self->sprite.graphic_id = SPRID_GAME_PLAYER_IDLE;
    self->sprite.flags |= SPRITE_FLAG_PLAYING;
    self->behavior = &behavior_player;

    *data = (player_data_s){0};

    entity_s *cursor = entity_alloc();
    if (!cursor) return;

    cursor->flags |= ENTITY_FLAG_KEEP_ON_ROOM_CHANGE;
    cursor->sprite.graphic_id = SPRID_GAME_PLATFORM_OUTLINE;
    cursor->sprite.palette = GFX_OBJPAL_USER1;
    cursor->sprite.ox = -1;
    cursor->sprite.zidx = -9;

    data->cursor = cursor;
    data->death_timer = -1;
}

static void behavior_player_free(entity_s *self)
{
    player_data_s *data = (player_data_s *)self->userdata;
    entity_free(data->cursor);
}

static bool player_simulate_droplet(int droplet_type, int dir, FIXED player_x,
                                    FIXED player_y, FIXED *out_x, FIXED *out_y)
{
    FIXED target_y = 0;
    FIXED vx = 0;
    FIXED vy = 0;

    FIXED px = player_x - int2fx(4);
    FIXED py = player_y - int2fx(4);

    if (droplet_type == PLAYER_DROPLET_TYPE_SIDE)
    {
        vx = PLAYER_SIDE_SPIT_VX * dir;
        vy = -PLAYER_SIDE_SPIT_VY;

        target_y = player_y + PLAYER_SIDE_SPIT_TARGET_Y_OFF;
    }
    else if (droplet_type == PLAYER_DROPLET_TYPE_SIDE_SLOW)
    {
        vx = PLAYER_SIDE_SPIT_CLOSE_VX * dir;
        vy = -PLAYER_SIDE_SPIT_VY;

        target_y = player_y + PLAYER_SIDE_SPIT_TARGET_Y_OFF;
    }
    else if (droplet_type == PLAYER_DROPLET_TYPE_UP)
    {
        vx = 0;
        vy = -PLAYER_UP_SPIT_VY;

        target_y = player_y - int2fx(8);
    }
    else LOG_ERR("invalid droplet type %i", droplet_type);

    target_y = fxmul(FX_FLOOR(fxdiv(target_y, WORLD_TILE_SIZE)), WORLD_TILE_SIZE);

    while (true)
    {
        vy += fxmul(WORLD_GRAVITY, PLAYER_SPIT_G_MULT);
        px += vx;
        py += vy;

        FIXED cx = FX_FLOOR(px + int2fx(4) + FIX_ONE / 2);
        FIXED cy = FX_FLOOR(py + int2fx(4) + FIX_ONE / 2);

        if (vy > 0 && cy >= target_y)
        {
            const FIXED x_snap = int2fx(WORLD_TILE_SIZE / 2);
            const FIXED tile_size = int2fx(WORLD_TILE_SIZE);
            
            FIXED plat_x = fxmul(FX_FLOOR(fxdiv(cx, x_snap) - FIX_ONE / 2 - 1), x_snap);
            FIXED plat_y = fxmul(FX_FLOOR(fxdiv(target_y, tile_size)), tile_size);
            plat_x = FX_FLOOR(plat_x) + FIX_ONE;
            plat_y = FX_FLOOR(plat_y);

            *out_x = plat_x;
            *out_y = plat_y;

            FIXED test_x = plat_x + int2fx(3);
            FIXED test_y = plat_y + int2fx(1);

            return (droplet_check_tile(test_x,             test_y) &&
                   droplet_check_tile(test_x + int2fx(3), test_y) &&
                   droplet_check_tile(test_x - int2fx(3), test_y));
        }
    }
}

static void player_platform_spit(entity_s *self)
{
    if (!(self->actor.flags & ACTOR_FLAG_GROUNDED)) return;
    if (g_game.player_ammo < 10) return;

    player_data_s *data = (player_data_s *)self->userdata;

    entity_s *droplet = entity_alloc();
    if (droplet)
    {
        g_game.player_ammo -= 10;
        snd_play(SND_ID_PLAYER_SPIT);

        int type, dir;
        FIXED x = self->pos.x + int2fx(3);
        FIXED y = self->pos.y + int2fx(4);
        
        if (key_is_down(KEY_UP))
        {
            type = PLAYER_DROPLET_TYPE_UP;
            dir = 0;
        }
        else
        {
            dir = (int) self->actor.face_dir;

            type = key_is_down(KEY_DOWN) ? PLAYER_DROPLET_TYPE_SIDE_SLOW
                                        : PLAYER_DROPLET_TYPE_SIDE;
        }

        entity_player_droplet_init(droplet, x, y, type, dir);
        data->spitting = true;
        self->sprite.graphic_id = SPRID_GAME_PLAYER_SPIT;
        self->sprite.accum = 0;
        self->sprite.frame = 0;
        self->sprite.flags |= SPRITE_FLAG_PLAYING;
        self->vel.x = 0;
    }
}

static void player_bullet_spit(entity_s *self)
{
    if (g_game.player_ammo < 1) return;
    --g_game.player_ammo;

    snd_play(SND_ID_PLAYER_SHOOT);

    projectile_s *proj = projectile_alloc();
    if (!proj) return;

    LOG_DBG("create player bullet");

    proj->px = self->pos.x + int2fx(self->col.w) / 2;
    proj->py = self->pos.y + int2fx(self->col.h) / 2;

    if (key_is_down(KEY_UP))
    {
        proj->vx = 0;
        proj->vy = TO_FIXED(-4.0);
    }
    else if (key_is_down(KEY_DOWN))
    {
        proj->vx = 0;
        proj->vy = TO_FIXED(4.0);
    }
    else
    {
        proj->vx = TO_FIXED(4.0) * self->actor.face_dir;
        proj->vy = 0;
    }

    proj->kind = PROJ_KIND_PLAYER;
    proj->graphic_id = SPRID_GAME_WATER_DROPLET;
    proj->life = 120;
}

static void behavior_player_death_update(entity_s *self)
{
    player_data_s *data = (player_data_s *)self->userdata;
    entity_s *cursor = data->cursor;
    if (cursor)
        cursor->sprite.flags |= SPRITE_FLAG_HIDDEN;

    if (++data->death_timer > 30)
        g_game.queue_restore = true;
}

static void behavior_player_update(entity_s *self)
{
    player_data_s *data = (player_data_s *)self->userdata;

    if (data->death_timer != -1)
    {
        behavior_player_death_update(self);
        return;
    }

    // input
    self->actor.move_x = 0;
    bool show_cursor = false;
    bool jumped = false;

    if (g_game.input_enabled && !g_game.room_trans.override_player_move_x)
    {
        bool can_move = !data->spitting;
        
#ifdef DEVDEBUG
        if (key_is_down(KEY_R))
        {
            can_move = false;

            self->vel.x = 0;
            self->vel.y = 0;
            self->flags &= ~(ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING);
            if (key_is_down(KEY_RIGHT))
                self->pos.x += int2fx(3);
            if (key_is_down(KEY_LEFT))
                self->pos.x -= int2fx(3);
            if (key_is_down(KEY_DOWN))
                self->pos.y += int2fx(3);
            if (key_is_down(KEY_UP))
                self->pos.y -= int2fx(3);
        } else self->flags |= (ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING);
#endif

        int actor_flags = (int) self->actor.flags;
        actor_flags &= ~ACTOR_FLAG_CAN_MOVE;
        if (can_move) actor_flags |= ACTOR_FLAG_CAN_MOVE;

        self->actor.flags = actor_flags;

        if (can_move)
        {
            int player_move_x = 0;

            if (key_is_down(KEY_RIGHT))
                ++player_move_x;

            if (key_is_down(KEY_LEFT))
                --player_move_x;

            self->actor.move_x = (s8) player_move_x;
        }

        if (key_hit(KEY_A))
        {
            self->actor.jump_trigger = 8;
            jumped = true;
        }

        if (can_move && !data->interactable)
        {
            show_cursor = (self->actor.flags & ACTOR_FLAG_GROUNDED) &&
                          g_game.player_spit_mode == PLAYER_SPIT_MODE_PLATFORM;
            if (key_hit(KEY_B))
            {
                if (g_game.player_spit_mode == PLAYER_SPIT_MODE_PLATFORM)
                    player_platform_spit(self);
                else if (g_game.player_spit_mode == PLAYER_SPIT_MODE_BULLET)
                    player_bullet_spit(self);
            }
        }

        if (key_hit(KEY_L))
        {
            if (++g_game.player_spit_mode >= 2)
                g_game.player_spit_mode = 0;
        }
    }

    if (g_game.room_trans.override_player_move_x)
        self->actor.move_x = g_game.room_trans.player_move_x;

    if (g_game.input_enabled && data->interactable)
    {
        if (key_hit(KEY_B))
            data->interactable->behavior->interact(data->interactable, self);

        g_game.active_interactable = data->interactable;
    }

    if (self->actor.move_x != 0)
    {
        int flags = self->sprite.flags;
        flags &= ~SPRITE_FLAG_FLIP_X;
        if (self->actor.move_x < 0)
            flags |= SPRITE_FLAG_FLIP_X;
        
        self->sprite.flags = flags & 0xFF;
    }

    if (self->actor.flags & ACTOR_FLAG_DID_JUMP)
        snd_play(SND_ID_PLAYER_JUMP);

    // animation
    if (data->spitting)
    {
        if (self->sprite.flags & SPRITE_FLAG_PLAYING)
            goto skip_animation;
        else
            data->spitting = false;
    }

    int anim = SPRID_GAME_PLAYER_IDLE;    
    if (self->actor.flags & ACTOR_FLAG_GROUNDED)
    {
        if (self->actor.move_x != 0)
        {
            anim = SPRID_GAME_PLAYER_WALK;
        }
        else
        {
            anim = SPRID_GAME_PLAYER_IDLE;
        }
    }
    else
    {
        anim = SPRID_GAME_PLAYER_IDLE;
    }

    if (self->sprite.graphic_id != anim)
    {
        self->sprite.graphic_id = anim;
        self->sprite.accum = 0;
        self->sprite.frame = 0;
        self->sprite.flags |= SPRITE_FLAG_PLAYING;
    }

    skip_animation:;

    data->interactable = NULL;
    
    entity_s *cursor = data->cursor;
    if (cursor)
    {
        bool valid_placement = false;

        if (show_cursor)
        {
            FIXED x = self->pos.x + int2fx(3);
            FIXED y = self->pos.y + int2fx(4);
            int type, dir;
            
            if (key_is_down(KEY_UP))
            {
                type = PLAYER_DROPLET_TYPE_UP;
                dir = 0;
            }
            else
            {
                dir = (int) self->actor.face_dir;

                type = key_is_down(KEY_DOWN) ? PLAYER_DROPLET_TYPE_SIDE_SLOW
                                            : PLAYER_DROPLET_TYPE_SIDE;
            }

            valid_placement =
                (player_simulate_droplet(type, dir, x, y, &cursor->pos.x,
                                         &cursor->pos.y));

            cursor->sprite.palette = valid_placement ? GFX_OBJPAL_USER1
                                                     : GFX_OBJPAL_MUL;
        }

        cursor->sprite.flags |= SPRITE_FLAG_HIDDEN;
        if (show_cursor && (valid_placement || data->cursor_frame == 0))
            cursor->sprite.flags &= ~SPRITE_FLAG_HIDDEN;
    }

    data->cursor_frame = (data->cursor_frame + 1) % 3;

    self->col.flags &= ~COL_FLAG_HEAD_BUMP;
    if (self->vel.y < 0 || jumped) self->col.flags |= COL_FLAG_HEAD_BUMP;

    if (self->col.flags & COL_FLAG_IN_WATER)
    {
        self->flags &= ~ENTITY_FLAG_ACTOR;
        self->flags |= ENTITY_FLAG_DAMPING;
        self->damp = TO_FIXED(0.8);
        self->sprite.graphic_id = SPRID_GAME_PLAYER_FROZEN;
        self->sprite.frame = 0;
        self->sprite.accum = 0;
        data->death_timer = 0;
        g_game.player_is_dead = true;
        snd_play(SND_ID_PLAYER_DIE);
    }
}

static void behavior_player_ent_touch(entity_s *self, entity_s *other, int nx,
                                      int ny)
{
    player_data_s *data = (player_data_s *)self->userdata;

    if (other->behavior && other->behavior->interact)
        data->interactable = other;
}

static void behavior_player_attacked(entity_s *self, entity_s *attacker,
                                     int dir)
{
    player_data_s *data = (player_data_s *)self->userdata;

    self->flags &= ~(ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR);

    // note: the original unyuland has a bug where the kinematics of fallen
    // enemies are ticked twice, so the numbers need to be adjusted here
    // since that bug is not present here.
    self->vel.x = TO_FIXED(0.5) * dir;
    self->vel.y = TO_FIXED(-1.5);
    self->gmult = TO_FIXED(1.2);

    self->sprite.graphic_id = SPRID_GAME_PLAYER_GOOPED;
    self->sprite.frame = 0;
    self->sprite.accum = 0;
    
    self->sprite.flags &= ~SPRITE_FLAG_FLIP_X;
    if (dir < 0)
        self->sprite.flags |= SPRITE_FLAG_FLIP_X;

    data->death_timer = 0;
    g_game.player_is_dead = true;
    
    snd_play(SND_ID_PLAYER_DIE);
}

static bool behavior_player_proj_touch(entity_s *self, projectile_s *proj)
{
    player_data_s *data = (player_data_s *)self->userdata;
    (void)data;

    if (proj->kind == PROJ_KIND_PLAYER)
        return true;

    behavior_player_attacked(self, NULL, sgn(proj->vx));
    return false;
}

const behavior_def_s behavior_player = {
    .free = behavior_player_free,
    .update = behavior_player_update,
    .ent_touch = behavior_player_ent_touch,
    .proj_touch = behavior_player_proj_touch,
    .attacked = behavior_player_attacked,
};

#pragma endregion player










////////////////////
// player_droplet //
////////////////////
#pragma region player_bullet

const behavior_def_s behavior_player_droplet;

typedef struct player_bullet_data
{
    u32 type;
    FIXED target_y;
}
player_bullet_data_s;

void entity_player_droplet_init(entity_s *self, FIXED px, FIXED py, int type,
                                int dir)
{
    player_bullet_data_s *data = (player_bullet_data_s *)&self->userdata;
    self->behavior = &behavior_player_droplet;
    self->flags |= ENTITY_FLAG_MOVING;
    self->gmult = PLAYER_SPIT_G_MULT;
    self->pos.x = px - int2fx(4);
    self->pos.y = py - int2fx(4);
    self->sprite.graphic_id = SPRID_GAME_WATER_DROPLET;
    self->sprite.zidx = -10;

    FIXED target_y = 0;

    if (type == PLAYER_DROPLET_TYPE_SIDE)
    {
        self->vel.x = PLAYER_SIDE_SPIT_VX * dir;
        self->vel.y = -PLAYER_SIDE_SPIT_VY;

        target_y = py + PLAYER_SIDE_SPIT_TARGET_Y_OFF;
    }
    else if (type == PLAYER_DROPLET_TYPE_SIDE_SLOW)
    {
        self->vel.x = PLAYER_SIDE_SPIT_CLOSE_VX * dir;
        self->vel.y = -PLAYER_SIDE_SPIT_VY;

        target_y = py + PLAYER_SIDE_SPIT_TARGET_Y_OFF;
    }
    else if (type == PLAYER_DROPLET_TYPE_UP)
    {
        self->vel.x = 0;
        self->vel.y = -PLAYER_UP_SPIT_VY;

        target_y = py - int2fx(8);
    }
    else LOG_ERR("invalid droplet type %i", type);

    data->type = type;
    data->target_y = fxmul(FX_FLOOR(fxdiv(target_y, WORLD_TILE_SIZE)), WORLD_TILE_SIZE);
}

static void behavior_player_droplet_update(entity_s *self)
{
    player_bullet_data_s *data = (player_bullet_data_s *)&self->userdata;

    FIXED cx = FX_FLOOR(self->pos.x + int2fx(4) + FIX_ONE / 2);
    FIXED cy = FX_FLOOR(self->pos.y + int2fx(4) + FIX_ONE / 2);

    if (self->vel.y > 0 && cy >= data->target_y)
    {
        const FIXED x_snap = int2fx(WORLD_TILE_SIZE / 2);
        const FIXED tile_size = int2fx(WORLD_TILE_SIZE);

        // FIXED cy = self->pos.y + int2fx(WORLD_TILE_SIZE / 2);

        // local offset = 0.0
        // if self.droplet_type == "up_platform" then
        //  offset = -0.5
        // end
        // local px = math.floor(position.x / x_snap + offset) * x_snap + 4.0
        // local py = math.floor(self.target_y / TILE_SIZE) * TILE_SIZE + 1.0
        // px, py = math.round(px), math.round(py)

        // offset controls whether it rounds to the nearest tile or always
        // downwards.
        // int offset = 0;

        // int offset = data->type == PLAYER_DROPLET_TYPE_UP
        //                  ? 0
        //                  : (FIXED)(FIX_ONE * 0.5);
        // not sure why i have to subtract one (aka 0.00390625),
        // but sure.
        FIXED px = fxmul(FX_FLOOR(fxdiv(cx, x_snap) - FIX_ONE / 2 - 1), x_snap);
        FIXED py = fxmul(FX_FLOOR(fxdiv(data->target_y, tile_size)), tile_size);
        px = FX_FLOOR(px) + FIX_ONE;
        py = FX_FLOOR(py);

        FIXED test_x = px + int2fx(3);
        FIXED test_y = py + int2fx(1);

        bool valid = (droplet_check_tile(test_x,             test_y) &&
                     droplet_check_tile(test_x + int2fx(3), test_y) &&
                     droplet_check_tile(test_x - int2fx(3), test_y));

        // newly created platform is free to override this entity slot.
        // obviously, self is now an invalid pointer, so don't dereference it
        // from this point forward.
        entity_free(self);

        if (valid)
        {
            entity_s *platf = entity_alloc();
            platf->flags |= ENTITY_FLAG_COLLIDE | ENTITY_FLAG_REMOVE_ON_CHECKPOINT;
            platf->pos.x = px;
            platf->pos.y = py;
            platf->col.w = 6;
            platf->col.h = 2;
            platf->col.group = COLGROUP_DEFAULT;
            platf->col.flags |= COL_FLAG_FLOOR_ONLY;
            platf->sprite.graphic_id = SPRID_GAME_ICE_PLATFORM;
            platf->sprite.ox = -1;
            platf->sprite.zidx = -10;

            snd_play(SND_ID_PLATFORM_PLACE);
        }

        return;
    }
}

const behavior_def_s behavior_player_droplet = {
    .update = behavior_player_droplet_update
};

#pragma endregion player_bullet










/////////////
// crawler //
/////////////
#pragma region crawler

const behavior_def_s behavior_crawler;

typedef struct crawler_data
{
    enemy_base_s base;
    FIXED max_dist;
    FIXED home_x;
}
crawler_data_s;

void entity_crawler_init(entity_s *self, FIXED px, FIXED py, FIXED max_dist)
{
    crawler_data_s *data = (crawler_data_s *)&self->userdata;
    self->behavior = &behavior_crawler;
    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR;
    self->pos.x = px + int2fx(1);
    self->pos.y = py;
    self->col.w = 6;
    self->col.h = 8;
    self->col.group = COLGROUP_ENTITY;
    self->col.mask = COLGROUP_DEFAULT | COLGROUP_PROJECTILE;
    self->sprite.graphic_id = SPRID_GAME_CRAWLER_WALK;
    self->sprite.flags |= SPRITE_FLAG_PLAYING;
    self->sprite.ox = -1;
    self->actor.face_dir = 1;
    self->actor.move_speed = TO_FIXED(0.5);
    self->actor.move_accel = TO_FIXED(1.0 / 8.0);
    
    *data = (crawler_data_s)
    {
        .base.health = 3,
        .max_dist = max_dist,
        .home_x = px
    };
}

static void behavior_crawler_update(entity_s *self)
{
    crawler_data_s *data = (crawler_data_s *)&self->userdata;
    if (enemy_base_update(self)) return;

    int face_dir = (int) self->actor.face_dir;

    const FIXED x_from_home = (self->pos.x - data->home_x) * face_dir;
    
    if (self->actor.flags & ACTOR_FLAG_WALL || (data->max_dist != 0 && x_from_home >= data->max_dist))
    {
        face_dir = -face_dir;
        self->actor.face_dir = face_dir;
    }
    
    self->actor.move_x = self->actor.face_dir;

    self->sprite.flags &= ~SPRITE_FLAG_FLIP_X;
    if (self->actor.move_x < 0)
        self->sprite.flags |= SPRITE_FLAG_FLIP_X;
}

static bool behavior_crawler_proj_touch(entity_s *self, projectile_s *proj)
{
    return enemy_base_proj_touch(self, proj, SPRID_GAME_CRAWLER_DEAD);
}

const behavior_def_s behavior_crawler = {
    .update = behavior_crawler_update,
    .proj_touch = behavior_crawler_proj_touch,
    .ent_touch = enemy_base_ent_touch
};

#pragma endregion crawler










///////////////
// gun_enemy //
///////////////
#pragma region gun_enemy

const behavior_def_s behavior_gun_enemy;

#define COS45 0.7071067811865475
#define GUN_ENEMY_SHOOT_COOLDOWN_LENGTH 50
#define GUN_ENEMY_PROJ_SPEED 2.0

typedef struct gun_enemy_data
{
    enemy_base_s base;
    u8 timer;
    bool ceil;
    int dir_flags;
}
gun_enemy_data_s;

void entity_gun_enemy_init(entity_s *self, FIXED px, FIXED py, bool ceil,
                           int dir_flags)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px + int2fx(1);
    self->pos.y = py;
    self->col.w = 6;
    self->col.h = 8;
    self->col.group = COLGROUP_ENTITY;
    self->col.mask = COLGROUP_DEFAULT | COLGROUP_PROJECTILE;
    self->sprite.graphic_id = SPRID_GAME_GUN_ENEMY_IDLE;
    self->sprite.ox = -1;

    if (ceil)
    {
        self->sprite.flags |= SPRITE_FLAG_FLIP_Y;
    }
    else
    {
        self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_DAMPING;
        self->sprite.oy = -4;
        self->damp = DEFAULT_DAMP;
    }

    self->behavior = &behavior_gun_enemy;

    gun_enemy_data_s *data = (gun_enemy_data_s *)self->userdata;
    *data = (gun_enemy_data_s)
    {
        .base.health = 3,
        .timer = GUN_ENEMY_SHOOT_COOLDOWN_LENGTH,
        .ceil = ceil,
        .dir_flags = dir_flags
    };
}

static void gun_enemy_shoot(FIXED x, FIXED y, FIXED vx, FIXED vy)
{
    projectile_s *proj = projectile_alloc();
    if (!proj) return;

    proj->px = x;
    proj->py = y;
    proj->vx = vx;
    proj->vy = vy;
    proj->graphic_id = SPRID_GAME_BULLET;
    proj->kind = PROJ_KIND_ENEMY;
    proj->life = 120;
}

static void behavior_gun_enemy_update(entity_s *self)
{
    gun_enemy_data_s *data = (gun_enemy_data_s *)&self->userdata;
    if (enemy_base_update(self)) return;

    --data->timer;
    if (data->timer == 0)
    {
        data->timer = GUN_ENEMY_SHOOT_COOLDOWN_LENGTH;

        FIXED px = self->pos.x + int2fx(3);
        FIXED py = self->pos.y + int2fx(4);
        FIXED yfac = data->ceil ? -1 : 1;
        int dir_flags = data->dir_flags;

        if (dir_flags & GUN_ENEMY_DIRFLAG_R)
            gun_enemy_shoot(px, py,
                            TO_FIXED(GUN_ENEMY_PROJ_SPEED),
                            TO_FIXED(0.0));

        if (dir_flags & GUN_ENEMY_DIRFLAG_TR)
            gun_enemy_shoot(px, py,
                            TO_FIXED(COS45 * GUN_ENEMY_PROJ_SPEED),
                            yfac * TO_FIXED(-COS45 * GUN_ENEMY_PROJ_SPEED));

        if (dir_flags & GUN_ENEMY_DIRFLAG_T)
            gun_enemy_shoot(px, py,
                            TO_FIXED(0),
                            yfac * TO_FIXED(-GUN_ENEMY_PROJ_SPEED));

        if (dir_flags & GUN_ENEMY_DIRFLAG_TL)
            gun_enemy_shoot(px, py,
                            TO_FIXED(-COS45 * GUN_ENEMY_PROJ_SPEED),
                            yfac * TO_FIXED(-COS45 * GUN_ENEMY_PROJ_SPEED));

        if (dir_flags & GUN_ENEMY_DIRFLAG_L)
            gun_enemy_shoot(px, py,
                            TO_FIXED(-GUN_ENEMY_PROJ_SPEED),
                            TO_FIXED(0.0));
        
        self->sprite.graphic_id = SPRID_GAME_GUN_ENEMY_FIRE;
        self->sprite.frame = 0;
        self->sprite.accum = 0;
        self->sprite.flags |= SPRITE_FLAG_PLAYING;

        int screen_dx = fx2int(self->pos.x) - (g_game.cam_x + SCREEN_WIDTH / 4);
        int screen_dy = fx2int(self->pos.y) - (g_game.cam_y + SCREEN_HEIGHT / 4);
        int screen_dist_sq = screen_dx * screen_dx + screen_dy * screen_dy;
        if (screen_dist_sq < 100 * 100)
            snd_play_no_overlap(SND_ID_ENEMY_SPIT);
    }
}

static bool behavior_gun_enemy_proj_touch(entity_s *self, projectile_s *proj)
{
    gun_enemy_data_s *data = (gun_enemy_data_s *)&self->userdata;
    bool res = enemy_base_proj_touch(self, proj, SPRID_GAME_GUN_ENEMY_DEAD);
    if (data->base.health < 0)
        self->sprite.flags &= ~SPRITE_FLAG_FLIP_Y;

    return res;
}

const behavior_def_s behavior_gun_enemy = {
    .update = behavior_gun_enemy_update,
    .proj_touch = behavior_gun_enemy_proj_touch,
    .ent_touch = enemy_base_ent_touch
};

#pragma endregion gun_enemy










///////////////
// ice_block //
///////////////
#pragma region ice_block

void entity_ice_block_init(entity_s *self, FIXED px, FIXED py)
{
    self->flags |=   ENTITY_FLAG_COLLIDE
                   | ENTITY_FLAG_MOVING
                   | ENTITY_FLAG_DAMPING;
    self->pos.x = px;
    self->pos.y = py;
    self->col.w = 8;
    self->col.h = 8;
    self->mass = 4;
    self->damp = DEFAULT_DAMP;
    self->sprite.graphic_id = SPRID_GAME_ICE_BLOCK;
}

#pragma endregion ice_block










////////////
// spring //
////////////
#pragma region spring

const behavior_def_s behavior_spring;

static void behavior_spring_ent_touch(entity_s *self, entity_s *other, int nx,
                                      int ny)
{
    if (!(other->flags & ENTITY_FLAG_MOVING))
        return;
    
    if (ny == -1)
    {
        other->vel.y = self->userdata[0] / (other->mass * 2) * 4;
        snd_play_no_overlap(SND_ID_SPRING);
    }
}

void entity_spring_init(entity_s *self, FIXED px, FIXED py, bool super)
{
    self->flags |= ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING
                   | ENTITY_FLAG_DAMPING;
    self->pos.x = px;
    self->pos.y = py;
    self->col.w = 8;
    self->col.h = 8;
    self->mass = 4;
    self->damp = DEFAULT_DAMP;
    self->sprite.graphic_id = super ? SPRID_GAME_SUPER_SPRING : SPRID_GAME_SPRING;
    self->behavior = &behavior_spring;
    self->userdata[0] = super ? TO_FIXED(-6.0) : TO_FIXED(-3.0);
}

const behavior_def_s behavior_spring = {
    .ent_touch = behavior_spring_ent_touch
};

#pragma endregion spring










//////////
// home //
//////////
#pragma region home
static const behavior_def_s behavior_home;

static void home_interact(entity_s *self, entity_s *source)
{
    LOG_DBG("home interacted with");
}

void entity_home_init(entity_s *self, FIXED px, FIXED py)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px;
    self->pos.y = py;
    self->col.w = 8;
    self->col.h = 8;
    self->col.flags |= COL_FLAG_MONITOR_ONLY;
    self->sprite.graphic_id = SPRID_GAME_HOME;
    self->sprite.ox = -4;
    self->sprite.oy = -16;
    self->sprite.zidx = -20;
    self->behavior = &behavior_home;
}

static const behavior_def_s behavior_home = {
    .interact = home_interact
};

#pragma endregion home










//////////
// sign //
//////////
#pragma region sign

const behavior_def_s behavior_sign;

typedef struct sign_data
{
    const char *dialogue;
}
sign_data_s;

void entity_sign_init(entity_s *self, FIXED px, FIXED py, const char *dialogue,
                      bool alt_appearance)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px;
    self->pos.y = py;
    self->col.w = 8;
    self->col.h = 8;
    self->col.flags = COL_FLAG_MONITOR_ONLY;
    self->sprite.graphic_id = alt_appearance ? SPRID_GAME_HINT_SIGN : SPRID_GAME_SIGN;
    self->sprite.zidx = -20;
    self->behavior = &behavior_sign;

    sign_data_s *data = (sign_data_s *)self->userdata;
    data->dialogue = dialogue;
}

static void entity_sign_interact(entity_s *self, entity_s *source)
{
    sign_data_s *data = (sign_data_s *)self->userdata;

    LOG_DBG("sign interact!");
    game_start_dialogue(data->dialogue);
}

const behavior_def_s behavior_sign = {
    .interact = entity_sign_interact
};

#pragma endregion sign










////////////////
// water tank //
////////////////
#pragma region water_tank

const behavior_def_s behavior_water_tank;

void entity_water_tank_init(entity_s *self, FIXED px, FIXED py)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px + int2fx(1);
    self->pos.y = py - int2fx(4);
    self->col.w = 6;
    self->col.h = 16;
    self->col.flags = COL_FLAG_MONITOR_ONLY;
    self->sprite.graphic_id = SPRID_GAME_WATER_TANK_NORMAL_INACTIVE;
    self->sprite.ox = -1;
    self->sprite.oy = -4;
    self->sprite.zidx = -20;
    self->behavior = &behavior_water_tank;
}

static void behavior_water_tank_interact(entity_s *self, entity_s *source)
{
    if (g_game.active_water_tank)
        g_game.active_water_tank->sprite.graphic_id
            = SPRID_GAME_WATER_TANK_NORMAL_INACTIVE;

    snd_play(SND_ID_CHECKPOINT);

    g_game.active_water_tank = self;
    g_game.player_ammo = 100;
    self->sprite.graphic_id = SPRID_GAME_WATER_TANK_NORMAL_ACTIVE;

    // respect "remove on checkpoint" flag
    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *e = g_game.entities + i;
        if (!ENTITY_ENABLED(e)) continue;

        if (e->flags & ENTITY_FLAG_REMOVE_ON_CHECKPOINT)
            entity_free(e);
    }
    
    // make sure player is centered perfectly on checkpoint when they respawn
    // TODO: position camera as well
    FIXED tmp_x = source->pos.x;
    FIXED tmp_y = source->pos.y;
    FIXED tmp_vx = source->vel.x;
    FIXED tmp_vy = source->vel.y;

    source->pos.x = self->pos.x;
    source->pos.y = self->pos.y;
    source->vel.x = 0;
    source->vel.y = 0;

    game_save_state();

    source->pos.x = tmp_x;
    source->pos.y = tmp_y;
    source->vel.x = tmp_vx;
    source->vel.y = tmp_vy;
}

const behavior_def_s behavior_water_tank = {
    .interact = behavior_water_tank_interact
};

#pragma endregion water_tank









///////////////////
// fragile_block //
///////////////////
#pragma region fragile_block
const behavior_def_s behavior_fragile_block;

typedef struct fragile_block_data
{
    int hits;
} fragile_block_data_s;

void entity_fragile_block_init(entity_s *self, FIXED px, FIXED py)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px;
    self->pos.y = py;
    self->col.w = 8;
    self->col.h = 8;
    self->sprite.graphic_id = SPRID_GAME_FRAGILE_BLOCK_RED;
    self->behavior = &behavior_fragile_block;

    fragile_block_data_s *data = (fragile_block_data_s *)self->userdata;
    data->hits = 0;
}

static bool behavior_fragile_block_proj_touch(entity_s *self,
                                              projectile_s *proj)
{
    fragile_block_data_s *data = (fragile_block_data_s *)self->userdata;
    LOG_DBG("block hit!!!");

    ++data->hits;
    ++self->sprite.frame;

    if (data->hits == 3)
        entity_queue_free(self);

    return false;
}

const behavior_def_s behavior_fragile_block = {
    .proj_touch = behavior_fragile_block_proj_touch
};

#pragma endregion









/////////
// orb //
/////////
#pragma region orb

static const behavior_def_s behavior_orb;

typedef struct orb_data
{
    uint frame;
    bool blue;
}
orb_data_s;

void entity_orb(entity_s *self, FIXED px, FIXED py, bool blue)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px + int2fx(2);
    self->pos.y = py;
    self->col.w = 4;
    self->col.h = 4;
    self->col.flags = COL_FLAG_MONITOR_ONLY;
    self->sprite.graphic_id = blue ? SPRID_GAME_FIRE_ORB_BLUE
                                   : SPRID_GAME_FIRE_ORB_RED;
    self->sprite.ox = -2;
    self->sprite.oy = -2;
    self->sprite.flags |= SPRITE_FLAG_PLAYING;
    self->behavior = &behavior_orb;

    orb_data_s *data = (orb_data_s *)self->userdata;
    data->frame = 0;
    data->blue = blue;
}

static void behavior_orb_update(entity_s *self)
{
    orb_data_s *data = (orb_data_s *)self->userdata;

    int oy = sine_lut(data->frame * 256 / 120) * 2 + 128;
    self->sprite.oy = -1 + (oy / 256);

    if (++data->frame == 120)
        data->frame = 120;
}

static void orb_display_dialogue(bool blue)
{
    const uint red_required = 4;
    const uint red_max = 5;
    const uint blue_max = 4;

    char *buf_ptr = game_dialogue_buffer;

    if (blue)
    {
        uint count = g_game.collected_borbs;
        if (count == blue_max)
        {
            buf_ptr += sprintf(buf_ptr, "Wow! You found all\nof the secret orbs!\f");
            buf_ptr += sprintf(buf_ptr, "Awesome!\f");
        }
        else
        {
            buf_ptr += sprintf(buf_ptr, "You found a secret\nfire orb!\f");
            buf_ptr += sprintf(buf_ptr, "%i more orbs left!\f", blue_max - count);
        }
    }
    else
    {
        uint count = g_game.collected_rorbs;

        if (count == red_max)
        {
            buf_ptr += sprintf(buf_ptr, "Wow! You found all\nred fire orbs!\f");
            buf_ptr += sprintf(buf_ptr, "You may return back\nhome to complete the\ngame.\f");
        }
        else if (count > red_required)
        {
            buf_ptr += sprintf(buf_ptr, "You found an extra\nfire orb!\f");
            buf_ptr += sprintf(buf_ptr, "You may return back\nhome to complete the\ngame.\f");
        }
        else if (count == red_required)
        {
            buf_ptr += sprintf(buf_ptr, "You found the last\nnecessary fire orb!\f");
            buf_ptr += sprintf(buf_ptr, "You may return back\nhome to complete the\ngame.\f");
        }
        else
        {
            buf_ptr += sprintf(buf_ptr, "You found a fire\norb!\f");
            buf_ptr += sprintf(buf_ptr, "%i more orbs left!\f", red_required - count);
        }
    }

    game_start_dialogue(game_dialogue_buffer);
}

static void behavior_orb_ent_touch(entity_s *self, entity_s *other, int nx,
                                   int ny)
{
    orb_data_s *data = (orb_data_s *)self->userdata;
    if (other != &g_game.entities[0]) return;

    LOG_DBG("Collect orab");

    if (data->blue)
        ++g_game.collected_borbs;
    else
        ++g_game.collected_rorbs;

    entity_queue_free(self);
    g_game.did_collect_orb = true;

    orb_display_dialogue(data->blue);
}

static const behavior_def_s behavior_orb = {
    .update = behavior_orb_update,
    .ent_touch = behavior_orb_ent_touch
};

#pragma endregion