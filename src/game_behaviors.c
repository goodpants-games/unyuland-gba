#include "game.h"
#include "log.h"
#include "tonc_math.h"

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
    self->sprite.graphic_id = GFXID_HOME;

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