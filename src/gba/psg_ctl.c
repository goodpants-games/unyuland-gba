#include <tonc.h>
#include <platutil.h>
#include "../main/log.h"

#include "psg_ctl.h"


static psg_tick_apply_f s_tick_apply = NULL;
static psg_tick_calc_f s_tick_calc = NULL;
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

    if (s_tick_apply)
        s_tick_apply(s_frame_tick_idx);

    ++s_frame_tick_idx;
    s_scanline_wait = s_scanline_wait_reset;
}

void psg_init(const psg_init_params_s *params)
{
    s_ticks_per_frame = params->ticks_per_frame;
    s_scanline_wait_reset = 228 / params->ticks_per_frame;
    s_tick_apply = params->tick_apply;
    s_tick_calc = params->tick_calc;
}

void psg_frame_start(void)
{
    if (s_tick_calc)
    {
        for (uint i = 0; i < s_ticks_per_frame; ++i)
            s_tick_calc(i);
    }
    
    s_frame_tick_idx = 0;
    s_scanline_wait = 0;
    irq_hblank();
}

ARM_FUNC void psg_irq_hblank(void)
{
    irq_hblank();
}