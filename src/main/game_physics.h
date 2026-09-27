/*
Copyright (C) 2026  pkhead

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef GAME_PHYSICS_H
#define GAME_PHYSICS_H

#include "game.h"

void game_physics_init(void);
void game_physics_update(void);

void game_physics_on_entity_alloc(entity_s *ent);
void game_physics_on_entity_free(entity_s *ent);
void game_physics_on_proj_alloc(projectile_s *proj);
void game_physics_on_proj_free(projectile_s *proj);

#endif