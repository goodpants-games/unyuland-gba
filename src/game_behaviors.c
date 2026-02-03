#include "game.h"
#include "log.h"
#include "tonc_math.h"
#include "tonc_input.h"

/////////////////////
// player_behavior //
/////////////////////

void entity_player_init(entity_s *self)
{
    self->flags |= ENTITY_FLAG_MOVING | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_ACTOR;
    self->col.w = 6;
    self->col.h = 8;
    self->actor.move_speed = (FIXED)(FIX_SCALE * 1);
    self->actor.move_accel = (FIXED)(FIX_SCALE / 8);
    self->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
    self->sprite.ox = -1;
    self->sprite.oy = -8;
    self->sprite.graphic_id = SPRID_GAME_PLAYER_IDLE;
    self->sprite.flags |= SPRITE_FLAG_PLAYING;

    self->behavior = &behavior_player;
}

static void behavior_player_update(entity_s *self)
{
    // input
    self->actor.move_x = 0;

    if (g_game.input_enabled)
    {
        int player_move_x = 0;

        if (key_is_down(KEY_RIGHT))
            ++player_move_x;

        if (key_is_down(KEY_LEFT))
            --player_move_x;

        self->actor.move_x = (s8) player_move_x;

        if (key_hit(KEY_B))
            self->actor.jump_trigger = 8;

        if (key_hit(KEY_A))
        {
            entity_s *droplet = entity_alloc();
            if (droplet)
            {
                int dir = (int) self->actor.face_dir;
                entity_player_droplet_init(droplet,
                                           self->pos.x, self->pos.y,
                                           PLAYER_DROPLET_TYPE_SIDE, dir);
            }
        }
    }

    // animation
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
}

const behavior_def_s behavior_player = {
    .update = behavior_player_update
};

///////////////////
// player_bullet //
///////////////////

#define PLAYER_SIDE_SPIT_VX           (FIXED)(FIX_ONE * 0.50)
#define PLAYER_SIDE_SPIT_CLOSE_VX     (FIXED)(FIX_ONE * 0.25)
#define PLAYER_SIDE_SPIT_VY           (FIXED)(FIX_ONE * 3.00)
#define PLAYER_SIDE_SPIT_TARGET_Y_OFF (FIXED)(FIX_ONE * 0.25)
#define PLAYER_UP_SPIT_VY             (FIXED)(FIX_ONE * 3.00)
#define PLAYER_SPIT_G_MULT            (FIXED)(FIX_ONE * 2.00)

typedef struct player_bullet_data
{
    s32 life;
}
player_bullet_data_s;

void entity_player_droplet_init(entity_s *self, FIXED px, FIXED py, int type,
                                int dir)
{
    player_bullet_data_s *data = (player_bullet_data_s *)&self->userdata;
    self->behavior = &behavior_player_droplet;
    self->flags |= ENTITY_FLAG_MOVING;
    self->gmult = PLAYER_SPIT_G_MULT;
    self->pos.x = px;
    self->pos.y = py;
    self->sprite.graphic_id = SPRID_GAME_WATER_DROPLET;

    if (type == PLAYER_DROPLET_TYPE_SIDE)
    {
        self->vel.x = PLAYER_SIDE_SPIT_VX * dir;
        self->vel.y = -PLAYER_SIDE_SPIT_VY;
    }
    else if (type == PLAYER_DROPLET_TYPE_UP)
    {
        self->vel.x = 0;
        self->vel.y = -PLAYER_UP_SPIT_VY;
    }
    else LOG_ERR("invalid droplet type %i", type);

    data->life = 60;
}

static void behavior_player_droplet_update(entity_s *self)
{
    player_bullet_data_s *data = (player_bullet_data_s *)&self->userdata;

    if (--data->life == 0)
    {
        LOG_DBG("DESTROY DROPLET");
        entity_free(self);
    }
}

const behavior_def_s behavior_player_droplet = {
    .update = behavior_player_droplet_update
};