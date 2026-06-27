#ifndef PSG_CTL
#define PSG_CTL

typedef void (*psg_tick_calc_f)(unsigned int tick_idx);
typedef void (*psg_tick_apply_f)(unsigned int tick_idx);

typedef struct psg_init_params
{
    int ticks_per_frame;
    psg_tick_calc_f tick_calc;
    psg_tick_apply_f tick_apply;
} psg_init_params_s;

void psg_init(const psg_init_params_s *params);

#ifdef PLATFORM_GBA
#include <platutil.h>
ARM_FUNC void psg_irq_hblank(void);
void psg_frame_start(void);
#endif

#ifdef PLATFORM_PC
#include <stdint.h>
#include <stddef.h>

void psg_set_sample_rate(int sr);
void psg_render(int16_t *out, size_t frame_count);
#endif

#endif