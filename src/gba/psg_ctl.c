#include <tonc.h>
#include <platutil.h>
#include "../main/log.h"

#include "psg_ctl.h"


static psg_tick_f s_tick_callback = NULL;
static int s_scanline_wait_reset;
static uint s_ticks_per_frame;
static uint s_frame_tick_idx = 0;
static uint s_scanline_wait = 0;


ARM_FUNC
static void irq_hblank(void)
{    
    if (s_frame_tick_idx == s_ticks_per_frame) return;
    if (s_scanline_wait-- != 0) return;

    // LOG_DBG("%x", s_tick_callback);

    if (s_tick_callback)
        s_tick_callback(s_frame_tick_idx);

    ++s_frame_tick_idx;
    s_scanline_wait = s_scanline_wait_reset;
}

void psg_init(int ticks_per_frame, psg_tick_f tick_callback)
{
    s_ticks_per_frame = ticks_per_frame;
    s_scanline_wait_reset = 228 / ticks_per_frame;
    s_tick_callback = tick_callback;
}

void psg_frame(void)
{
    s_frame_tick_idx = 0;
    s_scanline_wait = 0;
    irq_hblank();
}

ARM_FUNC void psg_irq_hblank(void)
{
    irq_hblank();
}