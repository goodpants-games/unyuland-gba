#include <tonc_math.h>
#include <tonc_input.h>
#include <tonc_video.h>
#include <assert.h>
#include <modplay.h>
#include <data/music.h>
#include <log.h>
#include <stdlib.h>
#include <string.h>

#include "dialogue.h"
#include "game.h"
#include "math_util.h"
#include "scenes.h"
#include "sound.h"
#include "printf.h"
#include "gfx.h"

#define DEFAULT_DAMP TO_FIXED(0.88)

// to be used in a constant-expression TO_FIXED/FX
// reminder to NEVER write code that makes the compiler emit FP procedures!!!
// (because gba has no hardware support for them; soft FP is very slow)
#define COS45 0.7071067811865475

#define PLAYER_SIDE_SPIT_VX           TO_FIXED(0.50)
#define PLAYER_SIDE_SPIT_CLOSE_VX     TO_FIXED(0.25)
#define PLAYER_SIDE_SPIT_VY           TO_FIXED(3.00)
#define PLAYER_SIDE_SPIT_TARGET_Y_OFF TO_FIXED(5.00)
#define PLAYER_UP_SPIT_VY             TO_FIXED(3.00)
#define PLAYER_SPIT_G_MULT            TO_FIXED(2.00)

#define EDATA_SIZE_CHECK(strt) \
    static_assert(sizeof(strt) <= sizeof(uintptr_t) * 4, \
                  "struct '" #strt "' exceeds storage capcity");

#define MSG_ID_BROKE_STALACTITE "brkstalc"
#define MSG_ID_STALACTITE_ATTACK "atkstalc"

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










//------------------------------------------------------------------------------
// GENERIC: enemy
//------------------------------------------------------------------------------
#pragma region G_enemy

typedef struct enemy_base
{
    s8 health;
    u8 hurt_flash;
}
enemy_base_s;
EDATA_SIZE_CHECK(enemy_base_s)

// if true, the calling function should return
static bool enemy_base_update(entity_s *self)
{
    enemy_base_s *data = (enemy_base_s *)&self->userdata;

    self->sprite.palette = ((data->hurt_flash >> 1) & 1)
                           ? GFX_OBJPAL_USER2 : GFX_OBJPAL_MUL;

    if (data->hurt_flash > 0)
        --data->hurt_flash;

    if (data->health < 0)
    {
        if (data->hurt_flash == 0)
            data->hurt_flash = 7;

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

    data->hurt_flash = 7;

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

        snd_play(SND_ID_ENEMY_HURT);
    }

    return false;
}

#pragma endregion G_enemy








//------------------------------------------------------------------------------
// player_behavior
//------------------------------------------------------------------------------
#pragma region player

const behavior_def_s behavior_player;

typedef struct player_data
{
    entity_s *interactable;
    entity_s *cursor;
    bool spitting;
    u8 cursor_frame;
    s8 death_timer;
    s8 spit_trigger;
} player_data_s;
EDATA_SIZE_CHECK(player_data_s)

void entity_player_init(entity_s *self)
{
    player_data_s *data = (player_data_s *)self->userdata;

    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR
                   | ENTITY_FLAG_KEEP_ON_ROOM_CHANGE;
    self->col.w = 6;
    self->col.h = 8;
    self->col.group = COLGROUP_ACTOR;
    self->col.mask = COLGROUP_DEFAULT | COLGROUP_PLR_PLAT | COLGROUP_PROJECTILE;
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

static bool player_platform_spit(entity_s *self)
{
    if (!(self->actor.flags & ACTOR_FLAG_GROUNDED)) return false;
    if (g_game.player_ammo < 10) return false;

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

    return true;
}

static bool player_bullet_spit(entity_s *self)
{
    if (g_game.player_ammo < 1) return false;
    --g_game.player_ammo;

    snd_play(SND_ID_PLAYER_SHOOT);

    projectile_s *proj = projectile_alloc();
    if (!proj) return false;

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

    return true;
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

    if (data->spit_trigger != 0)
        --data->spit_trigger;

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

        if (key_hit(KEY_B))
            data->spit_trigger = 8;

        if (can_move && !data->interactable)
        {
            show_cursor = (self->actor.flags & ACTOR_FLAG_GROUNDED) &&
                          g_game.player_spit_mode == PLAYER_SPIT_MODE_PLATFORM;
            
            bool did_spit = false;
            if (data->spit_trigger > 0)
            {
                if (g_game.player_spit_mode == PLAYER_SPIT_MODE_PLATFORM)
                    did_spit = player_platform_spit(self);
                else if (g_game.player_spit_mode == PLAYER_SPIT_MODE_BULLET)
                    did_spit = player_bullet_spit(self);
            }

            if (did_spit)
                data->spit_trigger = 0;
        }

#ifdef DEVDEBUG
        if (key_hit(KEY_L))
#else
        if (key_hit(KEY_L | KEY_R))
#endif
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
        data->spit_trigger = 0;
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

static void player_hit_by_stalactite(entity_s *self, entity_s *other)
{
    player_data_s *data = (player_data_s *)self->userdata;
    if (data->death_timer != -1) return;

    self->flags &= ~ENTITY_FLAG_ACTOR;
    self->vel.x = 0;
    self->vel.y = other->vel.y;

    self->sprite.graphic_id = SPRID_GAME_PLAYER_STALACTITE;
    self->sprite.frame = 0;
    self->sprite.accum = 0;

    data->death_timer = 0;
    g_game.player_is_dead = true;

    snd_play(SND_ID_PLAYER_DIE);
}

static void behavior_player_attacked(entity_s *self, entity_s *attacker,
                                     int dir)
{
    player_data_s *data = (player_data_s *)self->userdata;
    if (data->death_timer != -1) return;

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

static bool behavior_player_message(entity_s *self, const char *id, void *ud)
{
    (void)ud;

    if (!strcmp(id, MSG_ID_STALACTITE_ATTACK))
    {
        player_hit_by_stalactite(self, ud);
        return true;
    }

    return false;
}

const behavior_def_s behavior_player = {
    .free = behavior_player_free,
    .update = behavior_player_update,
    .ent_touch = behavior_player_ent_touch,
    .proj_touch = behavior_player_proj_touch,
    .attacked = behavior_player_attacked,
    .message = behavior_player_message
};

#pragma endregion player










//------------------------------------------------------------------------------
// player_droplet
//------------------------------------------------------------------------------
#pragma region player_droplet

const behavior_def_s behavior_player_droplet;

typedef struct player_bullet_data
{
    u32 type;
    FIXED target_y;
}
player_bullet_data_s;
EDATA_SIZE_CHECK(player_bullet_data_s)

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
            platf->col.h = 4;
            platf->col.group = COLGROUP_PLR_PLAT;
            platf->col.mask = COLGROUP_PROJECTILE;
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

#pragma endregion player_droplet










//------------------------------------------------------------------------------
// crawler
//------------------------------------------------------------------------------
#pragma region crawler

const behavior_def_s behavior_crawler;

typedef struct crawler_data
{
    enemy_base_s base;
    FIXED max_dist;
    FIXED home_x;
}
crawler_data_s;
EDATA_SIZE_CHECK(crawler_data_s)

void entity_crawler_init(entity_s *self, FIXED px, FIXED py, FIXED max_dist)
{
    crawler_data_s *data = (crawler_data_s *)&self->userdata;
    self->behavior = &behavior_crawler;
    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR;
    self->pos.x = px + int2fx(1);
    self->pos.y = py;
    self->col.w = 6;
    self->col.h = 8;
    self->col.group = COLGROUP_ACTOR;
    self->col.mask = COLGROUP_DEFAULT | COLGROUP_PLR_PLAT | COLGROUP_PROJECTILE;
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










//------------------------------------------------------------------------------
// gun_enemy
//------------------------------------------------------------------------------
#pragma region gun_enemy

const behavior_def_s behavior_gun_enemy;

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
EDATA_SIZE_CHECK(gun_enemy_data_s)

void entity_gun_enemy_init(entity_s *self, FIXED px, FIXED py, bool ceil,
                           int dir_flags)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px + int2fx(1);
    self->pos.y = py;
    self->col.w = 6;
    self->col.h = 8;
    self->col.group = COLGROUP_ACTOR;
    self->col.mask = COLGROUP_DEFAULT | COLGROUP_PLR_PLAT | COLGROUP_PROJECTILE;
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

        int screen_dx = fx2int(self->pos.x - g_game.cam_x);
        int screen_dy = fx2int(self->pos.y - g_game.cam_y);
        int screen_dist_sq = screen_dx * screen_dx + screen_dy * screen_dy;
        if (screen_dist_sq < 80 * 80)
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










//------------------------------------------------------------------------------
// ice_block
//------------------------------------------------------------------------------
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










//------------------------------------------------------------------------------
// spring
//------------------------------------------------------------------------------
#pragma region spring

typedef struct spring_data
{
    FIXED launch_vel;
}
spring_data_s;
EDATA_SIZE_CHECK(spring_data_s)

const behavior_def_s behavior_spring;

static void behavior_spring_ent_touch(entity_s *self, entity_s *other, int nx,
                                      int ny)
{
    if (!(other->flags & ENTITY_FLAG_MOVING))
        return;

    spring_data_s *data = (spring_data_s *)self->userdata;
    
    if (ny == -1)
    {
        other->vel.y = data->launch_vel / (other->mass * 2) * 4;
        snd_play_no_overlap(SND_ID_SPRING);
    }
}

void entity_spring_init(entity_s *self, FIXED px, FIXED py, bool super)
{
    spring_data_s *data = (spring_data_s *)self->userdata;

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
    data->launch_vel = super ? TO_FIXED(-6.0) : TO_FIXED(-3.0);
}

const behavior_def_s behavior_spring = {
    .ent_touch = behavior_spring_ent_touch
};

#pragma endregion spring










//------------------------------------------------------------------------------
// home
//------------------------------------------------------------------------------
#pragma region home
static const behavior_def_s behavior_home;

static void home_interact(entity_s *self, entity_s *source)
{
    if (g_game.collected_rorbs >= GAME_REQUIRED_RORBS)
    {
        scenemgr_change(&scene_desc_end, 0);
    }
    else
    {
        const char *dlg = dlg_get_chat_by_name("home_unfinished");
        game_start_dialogue(dlg);
    }
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










//------------------------------------------------------------------------------
// sign
//------------------------------------------------------------------------------
#pragma region sign

const behavior_def_s behavior_sign;

typedef struct sign_data
{
    const char *dialogue;
}
sign_data_s;
EDATA_SIZE_CHECK(sign_data_s);

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










//------------------------------------------------------------------------------
// water tank
//------------------------------------------------------------------------------
#pragma region water_tank

const behavior_def_s behavior_water_tank;

void entity_water_tank_init(entity_s *self, FIXED px, FIXED py)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px + int2fx(1);
    self->pos.y = py - int2fx(4);
    self->col.w = 6;
    self->col.h = 12;
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

    source->pos.x = self->pos.x + (int2fx(self->col.w) - int2fx(source->col.w)) / 2;
    source->pos.y = self->pos.y + int2fx(self->col.h) - int2fx(source->col.h);
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









//------------------------------------------------------------------------------
// fragile_block
//------------------------------------------------------------------------------
#pragma region fragile_block
const behavior_def_s behavior_fragile_block;

typedef struct fragile_block_data
{
    int hits;
} fragile_block_data_s;
EDATA_SIZE_CHECK(fragile_block_data_s)

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









//------------------------------------------------------------------------------
// orb
//------------------------------------------------------------------------------
#pragma region orb

static const behavior_def_s behavior_orb;

typedef struct orb_data
{
    uint frame;
    bool blue;
}
orb_data_s;
EDATA_SIZE_CHECK(orb_data_s)

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
    const uint red_required = GAME_REQUIRED_RORBS;
    const uint red_max = GAME_MAX_RORBS;
    const uint blue_max = GAME_MAX_BORBS;

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
    
    game_play_jingle(blue ? MOD_ORBGET2 : MOD_ORBGET);
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

#pragma endregion orb









//------------------------------------------------------------------------------
// stalactite
//------------------------------------------------------------------------------
#pragma region stalactite

static behavior_def_s behavior_stalactite;

typedef struct stalactite_data
{
    bool falling;
    u8 times_hit;
    u8 shake_timer;
    s8 sprite_ox;
}
stalactite_data_s;
EDATA_SIZE_CHECK(stalactite_data_s)

#define STALACTITE_EXTRA_WIDTH 4

void entity_stalactite_init(entity_s *self, FIXED px, FIXED py, int gfx_variant)
{
    self->flags |= ENTITY_FLAG_COLLIDE;
    self->pos.x = px - FX(STALACTITE_EXTRA_WIDTH) / 2;
    self->pos.y = py;
    self->col.w = 8 + STALACTITE_EXTRA_WIDTH;
    self->col.h = 16;
    self->col.group = 0;
    self->col.mask = COLGROUP_PROJECTILE;
    self->sprite.graphic_id = SPRID_GAME_STALACTITE;
    self->sprite.frame = gfx_variant - 1;
    self->sprite.ox = STALACTITE_EXTRA_WIDTH / 2;
    self->behavior = &behavior_stalactite;

    stalactite_data_s *data = (stalactite_data_s *)self->userdata;
    *data = (stalactite_data_s){
        .sprite_ox = STALACTITE_EXTRA_WIDTH / 2
    };
}

static bool stalactite_proj_touch(entity_s *self, projectile_s *proj)
{
    stalactite_data_s *data = (stalactite_data_s *)self->userdata;
    if (data->falling) return true;

    data->shake_timer = 8;

    // only player projectiles may cause it to fall
    if (proj->kind != PROJ_KIND_PLAYER) return true;
    snd_play_no_overlap(SND_ID_PLATFORM_PLACE);
    ++data->times_hit;

    // enable actor flag just so it can detect if it touched the ground
    // (sucks i know. but i'm too lazy at this point.)
    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_ACTOR;
    self->actor.flags |= ACTOR_FLAG_NO_VEL;
    self->col.mask = COLGROUP_DEFAULT;
    self->col.w = 8;
    self->pos.x += FX(STALACTITE_EXTRA_WIDTH) / 2;
    data->falling = true;
    data->sprite_ox = 0;

    game_send_global_message(MSG_ID_BROKE_STALACTITE, self);
    return false;
}

static void stalactite_ent_touch(entity_s *self, entity_s *other, int nx, int ny)
{
    stalactite_data_s *data = (stalactite_data_s *)self->userdata;
    if (!data->falling) return;
    if (!(other->col.group & COLGROUP_ACTOR)) return;

    if (other->behavior && other->behavior->message)
        other->behavior->message(other, MSG_ID_STALACTITE_ATTACK, self);

    if (other->behavior && other->behavior->attacked)
        other->behavior->attacked(other, self, 0);

    entity_queue_free(self);
}

static void stalactite_update(entity_s *self)
{
    stalactite_data_s *data = (stalactite_data_s *)self->userdata;

    if (data->shake_timer != 0)
    {
        uint anim_frame = data->shake_timer % 6;
        if (anim_frame < 3)
            self->sprite.ox = data->sprite_ox + 1;
        else
            self->sprite.ox = data->sprite_ox - 1;

        --data->shake_timer;
    }
    else
    {
        self->sprite.ox = data->sprite_ox;
    }

    if (!data->falling) return;

    if (self->actor.flags & ACTOR_FLAG_GROUNDED)
        entity_queue_free(self);
}

static behavior_def_s behavior_stalactite = {
    .proj_touch = stalactite_proj_touch,
    .ent_touch = stalactite_ent_touch,
    .update = stalactite_update
};

#pragma endregion stalactite









//------------------------------------------------------------------------------
// boss
//------------------------------------------------------------------------------
#pragma region boss

#define BOSS_JUMP_VEL           FX(2.5)
#define BOSS_ENT_FLAG_MASK      ENTITY_FLAG_DAMPING
#define BOSS_HEALTH             4

#define BOSS_STANDOFF_BASE_DIST_NORMAL FX(8 * 7)
#define BOSS_STANDOFF_BASE_DIST_FAR    FX(8 * 9)

#define BOSS_ARENA_LEFT  FX(16 * 8)
#define BOSS_ARENA_RIGHT FX(40 * 8)

#define BOSS_FLAG_CONTACT_DAMAGE (1 << 0)
#define BOSS_FLAG_DID_JUMP       (1 << 1)
#define BOSS_FLAG_WAS_ON_GROUND  (1 << 2)

/*
TODO: make him do the slide move less, and think of more moves he can do, that
obviously aren't as easy for the player to hit him with a stalactite.

- one where he shoots a wall(s) of projectiles (via jumping). that you need to
  weave around(?)
- charge and then high-jump to your position and GroundPound. (+ shockwave you
  need to dodge? that would require graphical dma effect?)

boss needs to make absolutely sure that the player cannot leave the boss arena
*/

typedef enum boss_mode
{
    BOSS_MODE_BACK,
    BOSS_MODE_PHASE0_IDLE,
    BOSS_MODE_SLIDE_WARN,
    BOSS_MODE_SLIDE,
    BOSS_MODE_SHOOT_JUMP_WARN,
    BOSS_MODE_SHOOT_JUMP,
    BOSS_MODE_HURT,

    BOSS_MODE_PHASE_DEFAULT = -1
}
boss_mode_e;

static const behavior_def_s behavior_boss;

typedef struct boss_data
{
    u8 mode;
    u8 flags;
    u8 global_counter;
    u8 hits;
    s16 wait_timer;

    union
    {
        s16 sub_timer;
        u8 phase_on_hurt;
        s16 back_desired_dist;
    };

    s16 standoff_base_dist;
}
boss_data_s;
EDATA_SIZE_CHECK(boss_data_s);

static void boss_switch_mode(entity_s *self, boss_mode_e mode);

void entity_boss_init(entity_s *self, FIXED px, FIXED py)
{
    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE |
                   ENTITY_FLAG_ACTOR | ENTITY_FLAG_GLOBAL_MSG;
    self->pos.x = px;
    self->pos.y = py;
    self->col.w = 16;
    self->col.h = 12;
    self->col.group = COLGROUP_ACTOR;
    self->col.mask = COLGROUP_DEFAULT | COLGROUP_PROJECTILE;
    self->actor.flags |= ACTOR_FLAG_NO_VEL;
    self->actor.move_speed = TO_FIXED(1.0);
    self->actor.move_accel = TO_FIXED(1.0 / 8.0);
    self->mass = 8;
    self->sprite.graphic_id = SPRID_GAME_BOSS_IDLE;
    self->sprite.oy = -4;
    self->behavior = &behavior_boss;

    boss_data_s *data = (boss_data_s *)self->userdata;
    *data = (boss_data_s){
        .standoff_base_dist = BOSS_STANDOFF_BASE_DIST_NORMAL
    };

    boss_switch_mode(self, BOSS_MODE_PHASE0_IDLE);
}

static projectile_s *boss_shoot(entity_s *self, FIXED px, FIXED py, FIXED dx,
                                FIXED dy, FIXED mul)
{
    projectile_s *proj = projectile_alloc();
    if (!proj) return NULL;

    proj->px = px;
    proj->py = py;
    proj->vx = fxmul(dx, mul);
    proj->vy = fxmul(dy, mul);
    proj->kind = PROJ_KIND_ENEMY;
    proj->graphic_id = SPRID_GAME_BULLET;
    proj->life = 90;

    return proj;
}

static inline uint boss_get_phase(entity_s *self)
{
    boss_data_s *const data = (boss_data_s *)self->userdata;

    // normal standoff
    if (data->hits >= 2) return 2;

    // standoff, but won't back off if player approaches or breaks a stalactite
    if (data->hits >= 1) return 1;

    // very aggressive
    return 0;
}

static void boss_switch_mode(entity_s *self, boss_mode_e mode)
{
    if (mode == BOSS_MODE_PHASE_DEFAULT)
    {
        uint phase = boss_get_phase(self);
        if (phase == 0)
            boss_switch_mode(self, BOSS_MODE_PHASE0_IDLE);
        else
            boss_switch_mode(self, BOSS_MODE_BACK);

        return;
    }

    boss_data_s *const data = (boss_data_s *)self->userdata;
    boss_mode_e cur_mode = data->mode;
    (void)cur_mode;

    entity_s *player = &g_game.entities[0];

    switch (mode)
    {
    case BOSS_MODE_BACK:
    {
        data->mode = BOSS_MODE_BACK;
        data->wait_timer = 0;

        data->standoff_base_dist =
            boss_get_phase(self) == 2 ? BOSS_STANDOFF_BASE_DIST_FAR
                                      : BOSS_STANDOFF_BASE_DIST_NORMAL;
        data->back_desired_dist = data->standoff_base_dist;
        break;
    }

    case BOSS_MODE_PHASE0_IDLE:
    {
        data->mode = BOSS_MODE_PHASE0_IDLE;
        data->wait_timer = 60;
        data->flags &= ~(BOSS_FLAG_DID_JUMP | BOSS_FLAG_CONTACT_DAMAGE);
        break;
    }

    case BOSS_MODE_SHOOT_JUMP_WARN:
    {
        snd_play(SND_ID_BOSS_JUMP_WINDUP);

        data->mode = BOSS_MODE_SHOOT_JUMP_WARN;
        data->wait_timer = 100;
        break;
    }

    case BOSS_MODE_SHOOT_JUMP:
    {
        data->mode = BOSS_MODE_SHOOT_JUMP;

        FIXED self_cx = self->pos.x + (FX(self->col.w) >> 1);
        FIXED dirx = SGN(player->pos.x - self->pos.x);
        FIXED target_x = player->pos.x + dirx * FX(8 * 10);
        FIXED jump_dist = target_x - self_cx;

        const FIXED max_jump_dist = FX(8 * 12);
        if (ABS(jump_dist) > max_jump_dist)
            jump_dist = max_jump_dist * SGN(jump_dist);

        FIXED airborne_duration = fxdiv(2 * BOSS_JUMP_VEL, WORLD_GRAVITY);
        self->vel.x = fxdiv(jump_dist, airborne_duration);
        self->vel.y = -BOSS_JUMP_VEL;

        data->wait_timer = 4;
        snd_play(SND_ID_BOSS_JUMP);
        break;
    }

    case BOSS_MODE_SLIDE_WARN:
    {
        data->wait_timer = 40;
        data->mode = BOSS_MODE_SLIDE_WARN;
        self->actor.move_x = SGN(player->pos.x - self->pos.x);
        snd_play_no_overlap(SND_ID_BOSS_DASH_WINDUP);
        break;
    }

    case BOSS_MODE_SLIDE:
    {
        data->wait_timer = 40;
        data->mode = BOSS_MODE_SLIDE;
        data->flags &= ~BOSS_FLAG_DID_JUMP;

        FIXED disp = self->pos.y - (player->pos.y - FX(8));
        if (disp < FX(4)) disp = FX(4);

        self->actor.jump_velocity = isqrt(2 * fxmul(disp, WORLD_GRAVITY)) * 16;
        snd_play_no_overlap(SND_ID_BOSS_DASH);
        break;
    }

    case BOSS_MODE_HURT:
    {
        data->mode = BOSS_MODE_HURT;
        data->flags &= ~BOSS_FLAG_CONTACT_DAMAGE;
        data->wait_timer = 80;
        snd_play_no_overlap(SND_ID_BOSS_HIT);
        break;
    }

    default: break;
    }
}

static void behavior_boss_update(entity_s *self)
{
    boss_data_s *const data = (boss_data_s *)self->userdata;

    self->actor.flags |= ACTOR_FLAG_CAN_MOVE | ACTOR_FLAG_NO_VEL;
    entity_s *player = &g_game.entities[0];

    u32 ent_flags = 0;
    bool is_grounded = self->actor.flags & ACTOR_FLAG_GROUNDED;

    if (is_grounded && !(data->flags & BOSS_FLAG_WAS_ON_GROUND))
        snd_play_no_overlap(SND_ID_BOSS_LAND);

    data->flags &= ~BOSS_FLAG_WAS_ON_GROUND;
    if (is_grounded) data->flags |= BOSS_FLAG_WAS_ON_GROUND;

    FIXED self_cx = self->pos.x + (FX(self->col.w) >> 1);
    FIXED self_cy = self->pos.y + (FX(self->col.h) >> 1) - FX(2);
    FIXED player_cx = player->pos.x + (FX(player->col.w) >> 1);
    FIXED player_cy = player->pos.y + (FX(player->col.h) >> 1);

    uint gfx_id = SPRID_GAME_BOSS_IDLE;
    bool clamp_position = true; // clamp position to bounds of arena
    data->flags |= BOSS_FLAG_CONTACT_DAMAGE;

    switch (data->mode)
    {
    case BOSS_MODE_BACK:
    {
        FIXED dx = player_cx - self_cx;
        int dirx = SGN(dx);

        if (data->back_desired_dist < FX(2 * 8) &&
            (player->actor.flags & ACTOR_FLAG_GROUNDED))
        {
            self->actor.move_x = dirx;
            boss_switch_mode(self, BOSS_MODE_SLIDE);
            break;
        }

        uint phase = boss_get_phase(self);

        self->actor.flags &= ~ACTOR_FLAG_NO_VEL;
        self->actor.move_speed = FX(1.0);
        self->actor.move_accel = FX(1.0 / 12.0);

        // if (data->sub_timer >= 0)
        // {
        //     if (self->actor.flags & ACTOR_FLAG_GROUNDED)
        //     {
        //         data->sub_timer = -1;
        //     }
        //     else
        //     {
        //         if (data->sub_timer == 0)
        //         {
        //             data->sub_timer = 0;
        //             FIXED shoot_y = self->pos.y + FX(self->col.h);
        //             boss_shoot(self, self_cx, shoot_y, FX(1), FX(0));
        //             snd_play_no_overlap(SND_ID_ENEMY_SPIT);
        //         }

        //         if (++data->sub_timer == 7)
        //             data->sub_timer = 0;
        //     }
        // }
        // else
        // {

        enum move_dir { NONE, TOWARD, AWAY } move_dir;

        if (ABS(dx) < data->back_desired_dist - FX(4))
        {
            // move away from player
            self->actor.move_x = -dirx;
            move_dir = AWAY;
        }
        else if (ABS(dx) > data->back_desired_dist + FX(4))
        {
            // move toward player
            self->actor.move_x = dirx;
            move_dir = TOWARD;
        }
        else
        {
            self->actor.move_x = 0;
            move_dir = NONE;
        }

        switch (move_dir)
        {
        case AWAY:
            if (phase == 2)
            {
                gfx_id = SPRID_GAME_BOSS_SCARED;
                data->back_desired_dist += FX(0.5);
                const s16 max_dist = data->standoff_base_dist;
                if (data->back_desired_dist > max_dist)
                    data->back_desired_dist = max_dist;

                data->flags &= ~BOSS_FLAG_CONTACT_DAMAGE;
            }
            else
            {
                data->back_desired_dist -= FX(0.2);
                self->pos.x += dirx * FX(0.2);
            }

            ++data->wait_timer;
            break;

        case NONE:
            data->back_desired_dist -= FX(0.2);
            self->pos.x += dirx * FX(0.2);

            ++data->wait_timer;
            break;

        case TOWARD:
            break;
        }

        uint fire_wait;
        if (phase == 1)
            fire_wait = 45;
        else
            fire_wait = data->hits == BOSS_HEALTH - 1 ? 37 : 42;

        if (data->wait_timer == fire_wait)
        {
            data->wait_timer = 0;

            boss_shoot(self, self_cx, self_cy - FX(4), FX(dirx), FX(0), FIX_ONE);
            boss_shoot(self, self_cx, self_cy + FX(4), FX(dirx), FX(0), FIX_ONE);
            snd_play(SND_ID_ENEMY_SPIT);
        }

        if (player_cy - self_cy < FX(-5))
        {
            FIXED disp = self->pos.y - (player->pos.y - FX(8));
            if (disp < FX(4)) disp = FX(4);

            self->actor.jump_velocity = isqrt(2 * fxmul(disp, WORLD_GRAVITY)) * 16;

            if (self->actor.jump_velocity < FX(2))
                self->actor.jump_velocity = FX(2);

            self->actor.jump_trigger = 1;
        }

        FIXED self_l = self->pos.x;
        FIXED self_r = self->pos.x + FX(self->col.w);

        if ((self->actor.flags & ACTOR_FLAG_GROUNDED) &&
            (self_l < BOSS_ARENA_LEFT || self_r > BOSS_ARENA_RIGHT))
        {
            LOG_DBG("backed into wall");
            boss_switch_mode(self, phase == 2 ? BOSS_MODE_SHOOT_JUMP_WARN : BOSS_MODE_SLIDE_WARN);
        }

        break;
    }

    case BOSS_MODE_PHASE0_IDLE:
    {
        ent_flags |= ENTITY_FLAG_DAMPING;
        self->damp = DEFAULT_DAMP;

        if (--data->wait_timer == 0)
        {
            if (data->global_counter == 2)
            {
                data->global_counter = 0;
                boss_switch_mode(self, BOSS_MODE_SHOOT_JUMP_WARN);
            }
            else
            {
                ++data->global_counter;
                boss_switch_mode(self, BOSS_MODE_SLIDE_WARN);
            }
        }
        break;
    }

    case BOSS_MODE_SLIDE_WARN:
    {
        if (self->actor.flags & ACTOR_FLAG_GROUNDED)
        {
            self->vel.x = self->actor.move_x * FX(-0.25);
            if (--data->wait_timer == 0)
                boss_switch_mode(self, BOSS_MODE_SLIDE);
        }
        
        clamp_position = false;
        break;
    }

    case BOSS_MODE_SLIDE:
    {
        self->vel.x = self->actor.move_x * FX(2);
        data->flags |= BOSS_FLAG_CONTACT_DAMAGE;

        FIXED player_dx = player_cx - self_cx;

        // if (!(data->flags & BOSS_FLAG_DID_JUMP) &&
        //     abs(player_dx) < FX(8 * 4))
        // {
        //     self->actor.jump_trigger = 1;
        //     data->flags |= BOSS_FLAG_DID_JUMP;
        // }

        uint phase = boss_get_phase(self);
        FIXED thresh;
        if (phase == 0)
            thresh = FX(8 * 4);
        else
        {
            thresh = FX(8 * 2);
            --data->wait_timer;
        }        

        FIXED dist_to_edge;
        if (self->actor.move_x == -1)
            dist_to_edge = abs(self->pos.x - BOSS_ARENA_LEFT);
        else
            dist_to_edge = abs(BOSS_ARENA_RIGHT - (self->pos.x + FX(self->col.w)));

        if (data->wait_timer == 0 || -player_dx * self->actor.move_x > thresh
            || dist_to_edge < FX(8 * 4))
        {
            boss_switch_mode(self, BOSS_MODE_PHASE_DEFAULT);
            break;
        }

        if (self->pos.x < BOSS_ARENA_LEFT)
        {
            self->pos.x = BOSS_ARENA_LEFT;
            self->vel.x = FX(3);
            self->vel.y = FX(-1);
            boss_switch_mode(self, BOSS_MODE_PHASE_DEFAULT);
            break;
        }
        
        clamp_position = false;
        break;
    }

    case BOSS_MODE_SHOOT_JUMP_WARN:
    {
        if (--data->wait_timer == 0)
        {
            boss_switch_mode(self, BOSS_MODE_SHOOT_JUMP);
        }
        break;
    }

    case BOSS_MODE_SHOOT_JUMP:
    {
        --data->wait_timer;
        if (data->wait_timer == 0)
        {
            FIXED spd = FX(0.5);
            boss_shoot(self, self_cx, self_cy,  FX(1),      FX(0),     spd);
            boss_shoot(self, self_cx, self_cy,  FX(-1),     FX(0),     spd);
            boss_shoot(self, self_cx, self_cy,  FX(0),      FX(-1),    spd);
            boss_shoot(self, self_cx, self_cy,  FX(0),      FX(1),     spd);
            boss_shoot(self, self_cx, self_cy,  FX(COS45),  FX(COS45), spd);
            boss_shoot(self, self_cx, self_cy, -FX(COS45),  FX(COS45), spd);
            boss_shoot(self, self_cx, self_cy,  FX(COS45), -FX(COS45), spd);
            boss_shoot(self, self_cx, self_cy, -FX(COS45), -FX(COS45), spd);
            snd_play_no_overlap(SND_ID_ENEMY_SPIT);

            data->wait_timer = 14;
        }

        if (self->actor.flags & ACTOR_FLAG_GROUNDED)
        {
            boss_switch_mode(self, BOSS_MODE_PHASE_DEFAULT);
        }
        
        break;
    }

    case BOSS_MODE_HURT:
    {
        ent_flags |= ENTITY_FLAG_DAMPING;
        self->damp = DEFAULT_DAMP;

        --data->wait_timer;
        int subf = data->wait_timer & 3;
        if (subf < 2)
            self->sprite.palette = GFX_OBJPAL_MUL;
        else
            self->sprite.palette = GFX_OBJPAL_USER2;

        if (data->wait_timer == 0)
        {
            uint phase = boss_get_phase(self);
            if (phase == 0)
            {
                boss_switch_mode(self, BOSS_MODE_PHASE_DEFAULT);
                data->wait_timer = 30;
            }
            else
            {
                if (data->phase_on_hurt < 1)
                    boss_switch_mode(self, BOSS_MODE_PHASE_DEFAULT);
                else
                    boss_switch_mode(self, BOSS_MODE_SLIDE_WARN);
            }
        }

        break;
    }
    }

    self->flags = (self->flags & ~BOSS_ENT_FLAG_MASK) | (ent_flags & BOSS_ENT_FLAG_MASK);

    if (self->sprite.graphic_id != gfx_id)
    {
        self->sprite.graphic_id = gfx_id;
        self->sprite.accum = 0;
        self->sprite.frame = 0;
        self->sprite.flags |= SPRITE_FLAG_PLAYING;
    }

    if (clamp_position)
    {
        if (self->pos.x < BOSS_ARENA_LEFT)
        {
            self->pos.x = BOSS_ARENA_LEFT;
            self->vel.x = 0;
        }
        else
        {
            FIXED self_w = FX(self->col.w);
            if (self->pos.x + self_w > BOSS_ARENA_RIGHT)
            {
                self->pos.x = BOSS_ARENA_RIGHT - self_w;
                self->vel.x = 0;
            }
        }
    }

    // if (self->actor.flags & ACTOR_FLAG_GROUNDED)
    //     --data->wait_timer;

    // if (data->wait_timer == 0)
    // {
    //     data->wait_timer = 60;
    //     FIXED dirx = SGN(player->pos.x - self->pos.x);
    //     FIXED target_x = player->pos.x + dirx * FX(80);
    //     // FIXED tx_inv = abs(fxdiv(FX(1), tx));

    //     boss_shoot(self, FX(1), FX(0));
    //     boss_shoot(self, FX(-1), FX(0));
    //     boss_shoot(self, FX(0), FX(-1));
    //     boss_shoot(self, FX(0), FX(1));

    //     FIXED airborne_duration = fxdiv(2 * BOSS_JUMP_VEL, WORLD_GRAVITY);
    //     self->vel.x = fxdiv(target_x - (self->pos.x + FX(4)), airborne_duration);
    //     self->vel.y = -BOSS_JUMP_VEL;

    //     snd_play_no_overlap(SND_ID_ENEMY_SPIT);
    // }
}

static bool behavior_boss_proj_touch(entity_s *self, projectile_s *proj)
{
    boss_data_s *data = (boss_data_s *)self->userdata;
    (void)data;

    if (proj->kind == PROJ_KIND_PLAYER)
    {
        FIXED cx = self->pos.x + (FX(self->col.w) >> 1);
        FIXED cy = self->pos.y + (FX(self->col.h) >> 1);
        FIXED dx = cx - proj->px;
        FIXED dy = cy - proj->py;
        // FIXED dlen = isqrt(fxmul(dx, dx) + fxmul(dy, dy)) * 16;
        // dx = fxdiv(dx, dlen);
        // dy = fxdiv(dy, dlen);

        FIXED dot = fxmul(dx, proj->vx) + fxmul(dy, proj->vy);
        if (dot > 0 && proj->g == 0)
        {
            self->pos.x += proj->vx / 32;
            self->pos.y += proj->vy / 32;

            proj->vx = SGN(proj->vx) * FX(-0.5);
            proj->vy = FX(-1);
            proj->g = FX(0.2);

            // proj->vx -= fxmul(dx, dot) * 2;
            // proj->vy -= fxmul(dy, dot) * 2;
            // proj->vx += self->vel.x;
            // proj->vy += self->vel.y;
        }
    }

    return true;
}

static void behavior_boss_ent_touch(entity_s *self, entity_s *other,
                                    int nx, int ny)
{
    boss_data_s *data = (boss_data_s *)self->userdata;
    (void)data;

    if (data->flags & BOSS_FLAG_CONTACT_DAMAGE)
        if (other->behavior && other->behavior->attacked)
            other->behavior->attacked(other, self, SGN3(self->vel.x));
}

static void behavior_boss_attacked(entity_s *self, entity_s *other, int dir)
{
    boss_data_s *data = (boss_data_s *)self->userdata;
    if (data->mode == BOSS_MODE_HURT) return;

    data->phase_on_hurt = boss_get_phase(self);
    ++data->hits;

    if (data->hits == BOSS_HEALTH)
    {
        game_start_dialogue("Boss is defeated\f");
    }

    boss_switch_mode(self, BOSS_MODE_HURT);
}

static bool behavior_boss_message(entity_s *self, const char *id,
                                  void *msg_data)
{
    // only reacts to stalactite broke message
    if (strcmp(id, MSG_ID_BROKE_STALACTITE)) return false;

    LOG_DBG("sensed broken stalactite");

    uint phase = boss_get_phase(self);

    boss_data_s *data = (boss_data_s *)self->userdata;
    if ((data->mode == BOSS_MODE_SLIDE || data->mode == BOSS_MODE_SLIDE_WARN)
        && phase >= 1)
    {
        LOG_DBG("stop sliding!");
        boss_switch_mode(self, BOSS_MODE_PHASE_DEFAULT);
    }
    else if (data->mode == BOSS_MODE_BACK && phase >= 2)
    {
        data->back_desired_dist = data->standoff_base_dist;
    }

    return true;
}

static const behavior_def_s behavior_boss = {
    .update = behavior_boss_update,
    .proj_touch = behavior_boss_proj_touch,
    .ent_touch = behavior_boss_ent_touch,
    .attacked = behavior_boss_attacked,
    .message = behavior_boss_message,
};

#pragma endregion boss