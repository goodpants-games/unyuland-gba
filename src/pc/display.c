#include "display.h"

#include <tonc.h>
#include <stdio.h>

#define DISPBUF g_display_buffer

u32 *g_display_buffer = NULL;
static u32 s_palette[256];

static const u16 *s_bg_ctl_addr_table[4] =
    {&REG_BG0CNT, &REG_BG1CNT, &REG_BG2CNT, &REG_BG3CNT};

static const u16 *s_bg_ctl_hofs[4] =
    {&REG_BG0HOFS, &REG_BG1HOFS, &REG_BG2HOFS, &REG_BG3HOFS};

static const u16 *s_bg_ctl_vofs[4] =
    {&REG_BG0VOFS, &REG_BG1VOFS, &REG_BG2VOFS, &REG_BG3VOFS};

static inline u32 r5g5b5a1_to_rgba32(u16 color)
{
    u8 r = color & 0x1F;
    u8 g = (color >> 5) & 0x1F;
    u8 b = (color >> 10) & 0x1F;
    
    // multiplier should be 8.22; max color component is now 248
    // eh. good enough.
    return (r * 8) | ((g * 8) << 8) | ((b * 8) << 16) | (0xFF000000);
}

static inline void blit_pixel(u32 *out, u32 *pal, uint pali)
{
    if (pali > 0) *out = pal[pali];
}

static void draw_bg(uint bg_idx, uint bg_prio)
{
    u16 bg_ctl = *(s_bg_ctl_addr_table[bg_idx]);

    uint prio = (bg_ctl & BG_PRIO_MASK) >> BG_PRIO_SHIFT;
    if (prio != bg_prio) return;

    uint cbb_idx = (bg_ctl & BG_CBB_MASK) >> BG_CBB_SHIFT;
    uint sbb_idx = (bg_ctl & BG_SBB_MASK) >> BG_SBB_SHIFT;

    uint hofs = (uint) *(s_bg_ctl_hofs[bg_idx]) & 0xFF;
    uint vofs = (uint) *(s_bg_ctl_vofs[bg_idx]) & 0xFF;
    uint hofs_frac = hofs & 7;
    uint vofs_frac = vofs & 7;

    TILE *tmem = tile_mem[cbb_idx];
    SCR_ENTRY *semem = se_mem[sbb_idx];

    uint se_col = vofs / 8;
    for (int y = 1; y < 20; ++y)
    {
        uint se_row = hofs / 8;
        for (int x = 1; x < 30; ++x)
        {
            u16 se_data = semem[se_col * 32 + se_row];
            se_row = (se_row + 1) & 31;

            uint tid = (se_data & SE_ID_MASK) >> SE_ID_SHIFT;
            uint pal_id = (se_data & SE_PALBANK_MASK) >> SE_PALBANK_SHIFT;
            u32 *tile = (u32 *)(tmem + tid);

            uint draw_x = x * 8 - hofs_frac;
            uint draw_y = y * 8 - vofs_frac;

            u32 *ocol = DISPBUF + (draw_y * SCREEN_WIDTH + draw_x);
            u32 *pal = s_palette + pal_id * 16;
            for (int iy = 0; iy < 8; ++iy)
            {
                u32 row = *(tile++);
                blit_pixel(ocol+0, pal, (row >> 0 ) & 0xF);
                blit_pixel(ocol+1, pal, (row >> 4 ) & 0xF);
                blit_pixel(ocol+2, pal, (row >> 8 ) & 0xF);
                blit_pixel(ocol+3, pal, (row >> 12) & 0xF);
                blit_pixel(ocol+4, pal, (row >> 16) & 0xF);
                blit_pixel(ocol+5, pal, (row >> 20) & 0xF);
                blit_pixel(ocol+6, pal, (row >> 24) & 0xF);
                blit_pixel(ocol+7, pal, (row >> 28) & 0xF);
                ocol += SCREEN_WIDTH;
            }
        }

        se_col = (se_col + 1) & 31;
    }
}

void display_update(void)
{
    if (DISPBUF == NULL) return;

    for (int i = 0; i < 256; ++i)
        s_palette[i] = r5g5b5a1_to_rgba32(pal_bg_mem[i]);

    // clear display buffer with background color
    const u32 bg_col = s_palette[0];
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        DISPBUF[i] = bg_col;

    bool bg_enabled[4];
    bg_enabled[0] = REG_DISPCNT & DCNT_BG0;
    bg_enabled[1] = REG_DISPCNT & DCNT_BG1;
    bg_enabled[2] = REG_DISPCNT & DCNT_BG2;
    bg_enabled[3] = REG_DISPCNT & DCNT_BG3;

    for (int prio = 3; prio >= 0; --prio)
    {
        if (bg_enabled[3]) draw_bg(3, prio);
        if (bg_enabled[2]) draw_bg(2, prio);
        if (bg_enabled[1]) draw_bg(1, prio);
        if (bg_enabled[0]) draw_bg(0, prio);
    }
}