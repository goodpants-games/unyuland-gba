#include <tonc.h>

u8 tonc__mem_io[0x400]       __attribute__((aligned(4)));
u8 tonc__mem_pal[PAL_SIZE]   __attribute__((aligned(4)));
u8 tonc__mem_vram[VRAM_SIZE] __attribute__((aligned(4)));
u8 tonc__mem_oam[OAM_SIZE]   __attribute__((aligned(4)));