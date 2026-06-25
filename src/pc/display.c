#include "display.h"

#include <tonc.h>
#include <stdio.h>

#define DISPBUF g_display_buffer

u32 *g_display_buffer = NULL;
static u32 s_bg_palette[256];
static u32 s_obj_palette[256];

static const u16 *s_bg_ctl_addr_table[4] =
    {&REG_BG0CNT, &REG_BG1CNT, &REG_BG2CNT, &REG_BG3CNT};

static const u16 *s_bg_ctl_hofs[4] =
    {&REG_BG0HOFS, &REG_BG1HOFS, &REG_BG2HOFS, &REG_BG3HOFS};

static const u16 *s_bg_ctl_vofs[4] =
    {&REG_BG0VOFS, &REG_BG1VOFS, &REG_BG2VOFS, &REG_BG3VOFS};

typedef struct sprite_shape
{
    u8 x, y;
}
sprite_shape_s;

static const sprite_shape_s s_sprite_shape_desc[16] = {
    // square
    { 1, 1 }, { 2, 2 }, { 4, 4 }, { 8, 8 },
    // horizontal
    { 2, 1 }, { 4, 1 }, { 4, 2 }, { 8, 4 },
    // vertical
    { 1, 2 }, { 1, 4 }, { 2, 4 }, { 4, 8 },
    // unused?
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
};

static inline u32 r5g5b5a1_to_rgba32(u16 color)
{
    u8 r = color & 0x1F;
    u8 g = (color >> 5) & 0x1F;
    u8 b = (color >> 10) & 0x1F;
    
    // multiplier should be 8.22; max color component is now 248
    // eh. good enough.
    return (r * 8) | ((g * 8) << 8) | ((b * 8) << 16) | (0xFF000000);
}

static inline void blit_pixel(u32 *out, int out_idx, u32 *pal, uint pali)
{
    if (out_idx < 0 || out_idx >= SCREEN_WIDTH) return;
    if (pali > 0) out[out_idx] = pal[pali];
}

static void draw_tile(TILE *p_tile, int draw_x, int draw_y, u32 *pal,
                     bool flip_x, bool flip_y)
{
    u32 *tile = p_tile->data;
    int tile_ptr_delta = 1;

    if (flip_y)
    {
        tile_ptr_delta = -1;
        tile += 7;
    }

    for (int i = 0; i < 8; ++i)
    {
        if (draw_y >= 0 && draw_y < SCREEN_HEIGHT)
        {
            u32 *ocol = DISPBUF + draw_y * SCREEN_WIDTH;
            u32 row = *tile;

            if (flip_x)
            {
                row = ((row & 0xFFFF0000) >> 16) | ((row & 0x0000FFFF) << 16);
                row = ((row & 0xFF00FF00) >> 8) | ((row & 0x00FF00FF) << 8);
                row = ((row & 0xF0F0F0F0) >> 4) | ((row & 0x0F0F0F0F) << 4);
            }

            blit_pixel(ocol, draw_x + 0, pal, (row >> 0 ) & 0xF);
            blit_pixel(ocol, draw_x + 1, pal, (row >> 4 ) & 0xF);
            blit_pixel(ocol, draw_x + 2, pal, (row >> 8 ) & 0xF);
            blit_pixel(ocol, draw_x + 3, pal, (row >> 12) & 0xF);
            blit_pixel(ocol, draw_x + 4, pal, (row >> 16) & 0xF);
            blit_pixel(ocol, draw_x + 5, pal, (row >> 20) & 0xF);
            blit_pixel(ocol, draw_x + 6, pal, (row >> 24) & 0xF);
            blit_pixel(ocol, draw_x + 7, pal, (row >> 28) & 0xF);
        }

        tile += tile_ptr_delta;
        ++draw_y;
    }
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
    for (int y = 0; y <= 20; ++y)
    {
        uint se_row = hofs / 8;
        for (int x = 0; x <= 30; ++x)
        {
            u16 se_data = semem[se_col * 32 + se_row];
            se_row = (se_row + 1) & 31;

            uint tid = (se_data & SE_ID_MASK) >> SE_ID_SHIFT;
            uint pal_id = (se_data & SE_PALBANK_MASK) >> SE_PALBANK_SHIFT;
            bool flip_x = se_data & SE_HFLIP;
            bool flip_y = se_data & SE_VFLIP;

            TILE *tile = tmem + tid;

            int draw_x = x * 8 - hofs_frac;
            int draw_y = y * 8 - vofs_frac;

            u32 *pal = s_bg_palette + pal_id * 16;
            draw_tile(tile, draw_x, draw_y, pal, flip_x, flip_y);
        }

        se_col = (se_col + 1) & 31;
    }
}

static inline int sextend32(uint num, uint b)
{
    int m = 1U << (b - 1);
    num &= (1U << b) - 1;
    return (num ^ m) - m;
}

static void draw_obj(OBJ_ATTR *obj)
{
    int spr_y = ((obj->attr0 & ATTR0_Y_MASK) >> ATTR0_Y_SHIFT);
    if (spr_y > 160)
        spr_y -= 256;

    int spr_x = sextend32((obj->attr1 & ATTR1_X_MASK) >> ATTR1_X_SHIFT, 9);
    uint tid = (obj->attr2 & ATTR2_ID_MASK) >> ATTR2_ID_SHIFT;
    uint pal_id = (obj->attr2 & ATTR2_PALBANK_MASK) >> ATTR2_PALBANK_SHIFT;
    bool xflip = obj->attr1 & ATTR1_HFLIP;
    bool yflip = obj->attr1 & ATTR1_VFLIP;

    TILE *tile = tile_mem_obj[0] + tid;
    u32 *pal = s_obj_palette + pal_id * 16;

    uint shape_lo = (obj->attr1 & ATTR1_SIZE_MASK) >> ATTR1_SIZE_SHIFT;
    uint shape_hi = (obj->attr0 & ATTR0_SHAPE_MASK) >> ATTR0_SHAPE_SHIFT;

    sprite_shape_s shape = s_sprite_shape_desc[(shape_hi << 2) | shape_lo];
    int sx = xflip ? -1 : 1;
    int ox = xflip ? (shape.x - 1) * 8 : 0;
    int sy = yflip ? -1 : 1;
    int oy = yflip ? (shape.y - 1) * 8 : 0;
    
    for (int y = 0; y < shape.y; ++y)
    {
        for (int x = 0; x < shape.x; ++x)
        {
            int tx = spr_x + x * 8 * sx + ox;
            int ty = spr_y + y * 8 * sy + oy;
            draw_tile(tile, tx, ty, pal, xflip, yflip);
            ++tile;
        }
    }
}

void display_update(void)
{
    if (DISPBUF == NULL) return;

    for (int i = 0; i < 256; ++i)
        s_bg_palette[i] = r5g5b5a1_to_rgba32(pal_bg_mem[i]);

    for (int i = 0; i < 256; ++i)
        s_obj_palette[i] = r5g5b5a1_to_rgba32(pal_obj_mem[i]);

    // clear display buffer with background color
    const u32 bg_col = s_bg_palette[0];
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
        DISPBUF[i] = bg_col;

    bool bg_enabled[4];
    bg_enabled[0] = REG_DISPCNT & DCNT_BG0;
    bg_enabled[1] = REG_DISPCNT & DCNT_BG1;
    bg_enabled[2] = REG_DISPCNT & DCNT_BG2;
    bg_enabled[3] = REG_DISPCNT & DCNT_BG3;

    // sprite list categorized by priority
    u8 sprite_list[4][128];
    u8 sprite_list_sz[4] = { 0, 0, 0, 0 };

    for (int si = 127; si >= 0; --si)
    {
        OBJ_ATTR *obj = oam_mem + si;
        if (obj->attr0 & ATTR0_HIDE) continue;

        int p = (obj->attr2 & ATTR2_PRIO_MASK) >> ATTR2_PRIO_SHIFT;
        sprite_list[p][sprite_list_sz[p]++] = (u8) si;
    }

    for (int prio = 3; prio >= 0; --prio)
    {
        if (bg_enabled[3]) draw_bg(3, prio);
        if (bg_enabled[2]) draw_bg(2, prio);
        if (bg_enabled[1]) draw_bg(1, prio);
        if (bg_enabled[0]) draw_bg(0, prio);

        int slsz = (int) sprite_list_sz[prio];
        for (int sli = 0; sli < slsz; ++sli)
            draw_obj(oam_mem + sprite_list[prio][sli]);
    }
}