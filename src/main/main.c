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

#include <tonc.h>
#include <modplay.h>
#include <psg_ctl.h>
#include <platutil.h>
#include <log.h>

#include "gfx.h"
#include "scenes.h"
#include "sound.h"

// #define MAIN_PROFILE

void platform_app_init(void)
{
    LOG_INIT();

#ifdef PLATFORM_GBA
    irq_init(NULL);
    irq_add(II_VBLANK, mplay_vblank_handler);
    irq_add(II_HBLANK, psg_irq_hblank);
#endif

    gfx_init();
    mplay_init();
    snd_init();

    scenemgr_init(&scene_desc_menu, 0);

    scenemgr_frame();
}

void platform_app_frame(void)
{
    #ifdef MAIN_PROFILE
    profile_start();
    #endif

    gfx_new_frame();

    key_poll();
    scenemgr_frame();
    
    #ifdef MAIN_PROFILE
    uint frame_len = profile_stop();
    LOG_DBG("frame usage: %.1f%%", (float)frame_len / 280896.f * 100.f);
    #endif
}
