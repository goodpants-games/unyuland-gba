#ifndef PSG_CTL
#define PSG_CTL

typedef void (*psg_tick_f)(int tick_index);

void psg_init(int ticks_per_frame, psg_tick_f tick_callback);
void psg_frame(void);

#ifdef PLATFORM_GBA
#include <platutil.h>
ARM_FUNC void psg_irq_hblank(void);
#endif

#endif