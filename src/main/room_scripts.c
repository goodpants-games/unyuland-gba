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