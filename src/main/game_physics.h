#ifndef GAME_PHYSICS_H
#define GAME_PHYSICS_H

#include "game.h"

void game_physics_init(void);
void game_physics_update(void);
// invalidates entire world state for next update call
void game_physics_invalidate(void);

void game_physics_on_entity_alloc(entity_s *ent);
void game_physics_on_entity_free(entity_s *ent);
void game_physics_on_proj_alloc(projectile_s *proj);
void game_physics_on_proj_free(projectile_s *proj);

#endif