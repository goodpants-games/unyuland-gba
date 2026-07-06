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
