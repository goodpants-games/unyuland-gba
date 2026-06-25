#include "display.h"

#include <tonc.h>
#include <stdio.h>

#define DISPBUF g_display_buffer

#define CLIP_MASK_WINOUT 1
#define CLIP_MASK_WIN0   2
#define CLIP_MASK_WIN1   4
#define CLIP_MASK_WININ  (CLIP_MASK_WIN0 | CLIP_MASK_WIN1)

typedef struct sprite_shape
{
    u8 x, y;
} sprite_shape_s;

typedef struct object_bins
{
    u8 bins[4][128];
    u8 bin_sz[4];
} object_bins_s;

u32 *g_display_buffer = NULL;
static u32 s_bg_palette[256];
static u32 s_obj_palette[256];

static const u16 *s_bg_ctl_addr_table[4] =
    {&REG_BG0CNT, &REG_BG1CNT, &REG_BG2CNT, &REG_BG3CNT};

static const u16 *s_bg_ctl_hofs[4] =
    {&REG_BG0HOFS, &REG_BG1HOFS, &REG_BG2HOFS, &REG_BG3HOFS};

static const u16 *s_bg_ctl_vofs[4] =
    {&REG_BG0VOFS, &REG_BG1VOFS, &REG_BG2VOFS, &REG_BG3VOFS};

static u8 s_cur_clip_mask = 0;
static u8 s_clip_mask[SCREEN_HEIGHT][SCREEN_WIDTH];

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

static void fill_clip_mask_region(uint sx, uint sy, uint ex, uint ey, u8 value)
{
    for (uint y = sy; y < ey; ++y)
        memset(s_clip_mask[y] + sx, value, ex - sx);
}

// note: y is used purely for clipping and does not affect pixel index
// calculation. out should already be the row that the pixel is drawn in.
static inline void blit_pixel(u32 *out, int x, int y, u32 *pal, uint pali)
{
    if (x < 0 || x >= SCREEN_WIDTH) return;
    if (!(s_clip_mask[y][x] & s_cur_clip_mask)) return;

    if (pali > 0) out[x] = pal[pali];
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
        if (draw_y < 0 || draw_y >= SCREEN_HEIGHT)
            goto next_row;

        u32 *ocol = DISPBUF + draw_y * SCREEN_WIDTH;
        u32 row = *tile;

        if (flip_x)
        {
            row = ((row & 0xFFFF0000) >> 16) | ((row & 0x0000FFFF) << 16);
            row = ((row & 0xFF00FF00) >> 8) | ((row & 0x00FF00FF) << 8);
            row = ((row & 0xF0F0F0F0) >> 4) | ((row & 0x0F0F0F0F) << 4);
        }

        blit_pixel(ocol, draw_x + 0, draw_y, pal, (row >> 0 ) & 0xF);
        blit_pixel(ocol, draw_x + 1, draw_y, pal, (row >> 4 ) & 0xF);
        blit_pixel(ocol, draw_x + 2, draw_y, pal, (row >> 8 ) & 0xF);
        blit_pixel(ocol, draw_x + 3, draw_y, pal, (row >> 12) & 0xF);
        blit_pixel(ocol, draw_x + 4, draw_y, pal, (row >> 16) & 0xF);
        blit_pixel(ocol, draw_x + 5, draw_y, pal, (row >> 20) & 0xF);
        blit_pixel(ocol, draw_x + 6, draw_y, pal, (row >> 24) & 0xF);
        blit_pixel(ocol, draw_x + 7, draw_y, pal, (row >> 28) & 0xF);

        next_row:;
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

static void draw_scene(bool bg_enabled[4], bool obj_enabled,
                       object_bins_s *obj_bins)
{
    for (int prio = 3; prio >= 0; --prio)
    {
        if (bg_enabled[3]) draw_bg(3, prio);
        if (bg_enabled[2]) draw_bg(2, prio);
        if (bg_enabled[1]) draw_bg(1, prio);
        if (bg_enabled[0]) draw_bg(0, prio);

        if (obj_enabled)
        {
            int slsz = (int) obj_bins->bin_sz[prio];
            for (int sli = 0; sli < slsz; ++sli)
                draw_obj(oam_mem + obj_bins->bins[prio][sli]);
        }
    }
}

static void draw_window(uint ctl, bool bg_enabled[4], object_bins_s *obj_bins)
{
    bool bg_win_enabled[4];
    bool obj_enabled;

    bg_win_enabled[0] = bg_enabled[0] && (ctl & WIN_BG0);
    bg_win_enabled[1] = bg_enabled[1] && (ctl & WIN_BG1);
    bg_win_enabled[2] = bg_enabled[2] && (ctl & WIN_BG2);
    bg_win_enabled[3] = bg_enabled[3] && (ctl & WIN_BG3);
    obj_enabled = ctl & WIN_OBJ;
    draw_scene(bg_win_enabled, obj_enabled, obj_bins);
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

    // object list categorized by priority
    object_bins_s obj_bins = (object_bins_s){0};

    for (int si = 127; si >= 0; --si)
    {
        OBJ_ATTR *obj = oam_mem + si;
        if (obj->attr0 & ATTR0_HIDE) continue;

        int p = (obj->attr2 & ATTR2_PRIO_MASK) >> ATTR2_PRIO_SHIFT;
        obj_bins.bins[p][obj_bins.bin_sz[p]++] = (u8) si;
    }

    // REG_DISPCNT & DCNT_WIN0;
    // REG_DISPCNT & DCNT_WIN1;

    bool enable_win0 = REG_DISPCNT & DCNT_WIN0;
    bool enable_win1 = REG_DISPCNT & DCNT_WIN1;

    if (enable_win0 || enable_win1)
    {
        memset(s_clip_mask, CLIP_MASK_WINOUT, SCREEN_WIDTH * SCREEN_HEIGHT);

        int win0l = min(REG_WIN0L, SCREEN_WIDTH);
        int win0r = min(REG_WIN0R, SCREEN_WIDTH);
        int win0t = min(REG_WIN0T, SCREEN_HEIGHT);
        int win0b = min(REG_WIN0B, SCREEN_HEIGHT);

        int win1l = min(REG_WIN1L, SCREEN_WIDTH);
        int win1r = min(REG_WIN1R, SCREEN_WIDTH);
        int win1t = min(REG_WIN1T, SCREEN_HEIGHT);
        int win1b = min(REG_WIN1B, SCREEN_HEIGHT);

        fill_clip_mask_region(win1l, win1t, win1r, win1b, CLIP_MASK_WIN1);
        fill_clip_mask_region(win0l, win0t, win0r, win0b, CLIP_MASK_WIN0);

        uint win0_conf = REG_WININ & 0x0F;
        uint win1_conf = (REG_WININ >> 8) & 0x0F;
        uint winout_conf = REG_WINOUT & 0x0F;

        // draw window out
        s_cur_clip_mask = CLIP_MASK_WINOUT;
        draw_window(winout_conf, bg_enabled, &obj_bins);

        // draw window 1
        s_cur_clip_mask = CLIP_MASK_WIN1;
        draw_window(win1_conf, bg_enabled, &obj_bins);

        // draw window 0
        s_cur_clip_mask = CLIP_MASK_WIN0;
        draw_window(win0_conf, bg_enabled, &obj_bins);
    }
    else
    {
        s_cur_clip_mask = 0xFF;
        memset(s_clip_mask, 0xFF, SCREEN_WIDTH * SCREEN_HEIGHT);
        draw_scene(bg_enabled, true, &obj_bins);
    }
}