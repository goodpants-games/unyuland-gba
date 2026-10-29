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

#include <stddef.h>
#include <assert.h>

#include "room_scripts.h"
#include "game.h"
#include "math_util.h"

#pragma GCC diagnostic ignored "-Wreturn-type"

void CORO_FUNC(room_script_test)
{
    CORO_BEGIN(v, {
        entity_s *player;
    })

    v->player = &g_game.entities[0];

    LOG_DBG("room script activated");

    for (;;)
    {
        if (v->player->pos.x >= FX(43 * 8))
            break;
        CORO_YIELD()
    }

    LOG_DBG("player reached it.");

    CORO_END()
}