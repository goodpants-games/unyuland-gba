#include "game.h"
#include "log.h"
#include "tonc_math.h"
#include "tonc_input.h"
#include "math_util.h"

/////////////////////
// player_behavior //
/////////////////////

typedef struct player_data
{
    bool spitting;
} player_data_s;

void entity_player_init(entity_s *self)
{
    player_data_s *data = (player_data_s *)self->userdata;

    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR;
    self->col.w = 6;
    self->col.h = 8;
    self->actor.move_speed = TO_FIXED(1.0);
    self->actor.move_accel = TO_FIXED(1.0 / 8.0);
    self->actor.jump_velocity = TO_FIXED(2.0);
    self->sprite.ox = -1;
    self->sprite.oy = -8;
    self->sprite.graphic_id = SPRID_GAME_PLAYER_IDLE;
    self->sprite.flags |= SPRITE_FLAG_PLAYING;
    self->behavior = &behavior_player;

    *data = (player_data_s){0};
}

static void behavior_player_update(entity_s *self)
{
    player_data_s *data = (player_data_s *)self->userdata;

    // input
    self->actor.move_x = 0;

    if (g_game.input_enabled)
    {
        bool can_move = !data->spitting;

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

        if (key_hit(KEY_B))
            self->actor.jump_trigger = 8;

        if (key_hit(KEY_A) && self->actor.flags & ACTOR_FLAG_GROUNDED &&
            can_move)
        {
            entity_s *droplet = entity_alloc();
            if (droplet)
            {
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
    }

    if (self->actor.move_x != 0)
    {
        int flags = self->sprite.flags;
        flags &= ~SPRITE_FLAG_FLIP_X;
        if (self->actor.move_x < 0)
            flags |= SPRITE_FLAG_FLIP_X;
        
        self->sprite.flags = flags & 0xFF;
    }

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
}

const behavior_def_s behavior_player = {
    .update = behavior_player_update
};

///////////////////
// player_bullet //
///////////////////

#define PLAYER_SIDE_SPIT_VX           TO_FIXED(0.50)
#define PLAYER_SIDE_SPIT_CLOSE_VX     TO_FIXED(0.25)
#define PLAYER_SIDE_SPIT_VY           TO_FIXED(3.00)
#define PLAYER_SIDE_SPIT_TARGET_Y_OFF TO_FIXED(5.00)
#define PLAYER_UP_SPIT_VY             TO_FIXED(3.00)
#define PLAYER_SPIT_G_MULT            TO_FIXED(2.00)

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
        int offset = 0;
        // int offset = data->type == PLAYER_DROPLET_TYPE_UP
        //                  ? 0
        //                  : (FIXED)(FIX_ONE * 0.5);
        // not sure why i have to subtract one (aka 0.00390625),
        // but sure.
        FIXED px = fxmul(FX_FLOOR(fxdiv(cx, x_snap) - FIX_ONE / 2 - 1), x_snap);
        FIXED py = fxmul(FX_FLOOR(fxdiv(data->target_y, tile_size)), tile_size);
        px = FX_FLOOR(px) + FIX_ONE;
        py = FX_FLOOR(py);

        // newly created platform is free to override this entity slot.
        // obviously, self is now an invalid pointer, so don't dereference it
        // from this point forward.
        entity_free(self);
        
        entity_s *platf = entity_alloc();
        platf->flags |= ENTITY_FLAG_COLLIDE | ENTITY_FLAG_REMOVE_ON_CHECKPOINT;
        platf->pos.x = px;
        platf->pos.y = py;
        platf->col.w = 6;
        platf->col.h = 2;
        platf->col.flags |= COL_FLAG_FLOOR_ONLY;
        platf->sprite.graphic_id = SPRID_GAME_ICE_PLATFORM;
        platf->sprite.ox = -1;
        platf->sprite.zidx = -10;

        return;
    }
}

const behavior_def_s behavior_player_droplet = {
    .update = behavior_player_droplet_update
};