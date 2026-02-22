#ifndef GAME_PHYSICS_H
#define GAME_PHYSICS_H

#include "game.h"

void game_physics_init(void);
void game_physics_update(void);
void game_physics_on_proj_alloc(projectile_s *proj);
void game_physics_on_proj_free(projectile_s *proj);

#endif