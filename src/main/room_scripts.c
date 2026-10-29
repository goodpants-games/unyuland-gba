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
#include "dialogue.h"
#include "math_util.h"
#include "scenes.h"

#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wmultichar"

static behavior_def_s cur_door_behavior;

static entity_s *find_entity_by_tag(u32 tag)
{
    entity_s *dst = g_game.entities;
    for (uint i = MAX_ENTITY_COUNT; i > 0; --i, ++dst)
    {
        if (!ENTITY_ENABLED(dst)) continue;
        if (dst->tag == tag) return dst;
    }
    return NULL;
}

//------------------------------------------------------------------------------
// room script: test
//------------------------------------------------------------------------------
void CORO_FUNC(room_script_test)
{
    CORO_BEGINV(v, {
        entity_s *player;
        entity_s *npc;
        int i;
    })
    LOG_DBG("room script activated");

    v->player = &g_game.entities[0];
    // wait until player is at x >= 43
    for (;;)
    {
        if (v->player->pos.x >= FX(43 * 8))
            break;
        CORO_YIELD()
    }
    LOG_DBG("player reached it.");
    // create npc
    v->npc = entity_alloc();
    if (!v->npc) CORO_RETURN();
    v->npc->pos.x = FX(46 * 8);
    v->npc->pos.y = FX(18 * 8);
    v->npc->sprite.graphic_id = SPRID_GAME_MANGO_UNYU_IDLE;
    CORO_YIELD();
    // show dialogue
    const char *dlg = dlg_get_chat_by_name("unyu_dialogue_mango");
    if (!dlg)
    {
        LOG_ERR("could not find dialogue!");
        CORO_RETURN();
    }
    game_start_dialogue(dlg);
    CORO_YIELD();
    // after dialogue ends, the npc floats to the heavens and disappears...
    for (v->i = 0; v->i < 120; ++v->i)
    {
        v->npc->pos.y -= FX(1);
        CORO_YIELD();
    }
    entity_free(v->npc);

    CORO_END
}

//------------------------------------------------------------------------------
// room script: unyuhome
//------------------------------------------------------------------------------
static void unyuhome_door_interact(entity_s *self, entity_s *source)
{
    game_transition_to_room(&world_rooms[0]);
}

void CORO_FUNC(room_script_unyuhome)
{
    CORO_BEGIN

    cur_door_behavior.interact = unyuhome_door_interact;
    entity_s *ent = find_entity_by_tag('door');
    if (ent)
        ent->behavior = &cur_door_behavior;

    CORO_END
}

//------------------------------------------------------------------------------
// room script: b01
//------------------------------------------------------------------------------
static void b01_door_interact(entity_s *self, entity_s *source)
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

void CORO_FUNC(room_script_b01)
{
    CORO_BEGIN

    LOG_DBG("activate room_script_b01");

    cur_door_behavior.interact = b01_door_interact;
    entity_s *ent = find_entity_by_tag('door');
    if (ent)
        ent->behavior = &cur_door_behavior;

    CORO_END
}