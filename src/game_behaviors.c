#include "game.h"
#include "log.h"

///////////////////
// player_bullet //
///////////////////

typedef struct player_bullet_data
{
    s32 life;
} player_bullet_data_s;

void behavior_player_droplet_init(entity_s *self)
{
    player_bullet_data_s *data = (player_bullet_data_s *)&self->userdata;
    self->behavior = &behavior_player_droplet;

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