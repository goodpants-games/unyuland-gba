#ifndef PSG_CTL
#define PSG_CTL

#define SWSEL_DIM_MASK   32
#define SWSEL_DIM_SHIFT  5
#define SWSEL_DIM(n)     (((n) << SWSEL_DIM_SHIFT) & SWSEL_DIM_MASK)

#define SWSEL_BANK_MASK  64
#define SWSEL_BANK_SHIFT 6
#define SWSEL_BANK(n)    (((n) << SWSEL_BANK_SHIFT) & SWSEL_BANK_MASK)

#define SWSEL_ENABLE(n) (((n) & 1) << 7)
#define SWSEL_ON        SWSEL_ENABLE(1)
#define SWSEL_OFF       SWSEL_ENABLE(0)

#define SWAV_IVOL_MASK  8192
#define SWAV_IVOL_SHIFT 13
#define SWAV_IVOL(n)    (((n) << SWAV_IVOL_SHIFT) & SWAV_IVOL_MASK)

// Game doesn't use this it's too stinky. I don't understand it.
#define SLFSR_SHIFT(n) (((n) & 15) << 4)
#define SLFSR_DIV(n)   ((n) & 7)
#define SLFSR_WIDTH(n) (((n) & 1) << 3)

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
#include <tonc_memmap.h>
#include <tonc_memdef.h>

ARM_FUNC void psg_irq_hblank(void);
void psg_frame_start(void);

static inline void psg_set_wavsel(uint16_t v)
{
    REG_SND3SEL = v;
}
#endif

#ifdef PLATFORM_PC
#include <stdint.h>
#include <stddef.h>

void psg_set_sample_rate(int sr);
void psg_render(int16_t *out, size_t frame_count);
void psg_set_wavsel(uint16_t value);
#endif

#endif