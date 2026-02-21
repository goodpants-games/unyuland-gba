#include <tonc.h>
#include <ctype.h>
#include <font_sprdb_gfx.h>
#include "gfx.h"
#include "mgba.h"
#include "log.h"

#define TEXT_CHAR_ID(i) (((i) - 1) * 4)

OBJ_ATTR gfx_oam_buffer[128];
int gfx_scroll_x = 0;
int gfx_scroll_y = 0;
uint gfx_map_width = 0;
uint gfx_map_height = 0;
const map_header_s *gfx_loaded_map = NULL;

TILE gfx_text_bmp[GFX_TEXT_BMP_SIZE];

static uint old_scroll_x = 0;
static uint old_scroll_y = 0;
static bool screen_dirty = false;

u16 gfx_palette[16] = {
    0x0000,
    0x28a3,
    0x288f,
    0x2a00,
    0x1955,
    0x254b,
    0x6318,
    0x77df,
    0x241f,
    0x029f,
    0x13bf,
    0x1b80,
    0x7ea5,
    0x4dd0,
    0x55df,
    0x573f,
};

static void write_scr_block(const uint map_entry, u32 *const dest)
{
    if (map_entry == 0)
    {
        *dest        = 0;
        *(dest + 16) = 0;
        return;
    }

    int gfx_id = map_entry & 0xFF;
    int v = gfx_id % 16 * 2 + gfx_id / 16 * 64;
    v = (v - 1) + GFX_CHAR_GAME_TILESET / 2;

    u32 upper = ((v+1) << 16) | v;
    v += 32;
    u32 lower = ((v+1) << 16) | v;

    if (map_entry & 0x100) // horiz flip
    {
        upper = ((upper & 0xFFFF) << 16) | (upper >> 16);
        lower = ((lower & 0xFFFF) << 16) | (lower >> 16);
        upper = upper | SE_HFLIP | (SE_HFLIP << 16);
        lower = lower | SE_HFLIP | (SE_HFLIP << 16);
    }

    if (map_entry & 0x200) // vert flip
    {
        u32 temp = upper;
        upper = lower;
        lower = temp;
        upper |= SE_VFLIP | (SE_VFLIP << 16);
        lower |= SE_VFLIP | (SE_VFLIP << 16);
    }

    *dest = upper;
    *(dest + 16) = lower;;
}

void gfx_init(void)
{
    oam_init(gfx_oam_buffer, 128);
    gfx_reset_palette();

    REG_DISPCNT = DCNT_OBJ_1D;

    REG_BG0CNT = BG_CBB(0) | BG_SBB(GFX_BG0_INDEX) | BG_4BPP | BG_REG_32x32;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;

    REG_BG1CNT = BG_CBB(0) | BG_SBB(GFX_BG1_INDEX) | BG_8BPP | BG_REG_32x32;
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
}

void gfx_new_frame(void)
{
    REG_DISPCNT |= DCNT_OBJ | DCNT_BG0 | DCNT_BG1;

    if (gfx_loaded_map)
    {
        const u16 *map_data = map_graphics_data(gfx_loaded_map);
        u32 *se32 = (u32 *)se_mem[GFX_BG1_INDEX];

        int prev_cam_tx = old_scroll_x / 16;
        int cam_tx = gfx_scroll_x / 16;

        int prev_cam_ty = old_scroll_y / 16;
        int cam_ty = gfx_scroll_y / 16;

        if (screen_dirty)
        {
            screen_dirty = false;
            int ey = cam_ty + SCRH_T16 + 1;
            int ex = cam_tx + SCRW_T16 + 1;

            for (int y = cam_ty; y < ey; ++y)
            {
                for (int x = cam_tx; x < ex; ++x)
                {
                    uint ii = y * gfx_map_width + x;
                    uint oi = (y % 16) * 32 + (x % 16);
                    uint entry = (uint) map_data[ii];
                    write_scr_block(entry, se32 + oi);
                }
            }
        }
        else
        {
            // x scrolling (also handles corners)
            if (cam_tx != prev_cam_tx)
            {
                int sx, ex;
                if (cam_tx > prev_cam_tx)
                {
                    sx = prev_cam_tx + SCRW_T16 + 1;
                    ex = cam_tx + SCRW_T16;
                }
                else
                {
                    sx = cam_tx;
                    ex = prev_cam_tx;
                }

                int sy = cam_ty;
                int ey = cam_ty + SCRH_T16;

                for (int x = sx; x <= ex; ++x)
                {
                    for (int y = sy; y <= ey; ++y)
                    {
                        uint ii = y * gfx_map_width + x;
                        uint oi = (y % 16) * 32 + (x % 16);
                        uint entry = (uint) map_data[ii];
                        write_scr_block(entry, se32 + oi);
                    }
                }
            }

            // y scrolling
            if (cam_ty != prev_cam_ty)
            {
                int sy, ey;
                if (cam_ty > prev_cam_ty)
                {
                    sy = prev_cam_ty + SCRH_T16 + 1;
                    ey = cam_ty + SCRH_T16;
                }
                else
                {
                    sy = cam_ty - 1;
                    ey = prev_cam_ty - 1;
                }

                int sx = cam_tx;
                int ex = cam_tx + SCRW_T16;

                for (int y = sy; y <= ey; ++y)
                {
                    for (int x = sx; x <= ex; ++x)
                    {
                        uint ii = y * gfx_map_width + x;
                        uint oi = (y % 16) * 32 + (x % 16);
                        uint entry = (uint) map_data[ii];
                        write_scr_block(entry, se32 + oi);
                    }
                }
            }
        }
    }

    old_scroll_x = gfx_scroll_x;
    old_scroll_y = gfx_scroll_y;

    oam_copy(oam_mem, gfx_oam_buffer, 128);
    REG_BG1HOFS = gfx_scroll_x;
    REG_BG1VOFS = gfx_scroll_y;
}

void gfx_load_map(const map_header_s *map)
{
    gfx_loaded_map = map;
    gfx_map_width = gfx_loaded_map->width;
    gfx_map_height = gfx_loaded_map->height;
    screen_dirty = true;
}

void gfx_reset_palette(void)
{
    // 0-15: regular palette, may be darkened for room transition
    // 16: black (if on 16-bit color mode, this will be the start of the 2nd bank)
    // 17-32: regular palette, with idx15 (peach) swapped out for black.
    // will not be darkened during room trans. used for UI (presumably).
    for (int i = 0; i < 16; ++i)
        pal_bg_mem[i] = gfx_palette[i];
    pal_bg_mem[16] = gfx_palette[0];

    for (int i = 1; i < 16; ++i)
        pal_bg_mem[16 + i] = gfx_palette[i];
    pal_bg_mem[31] = gfx_palette[0];

    // pal bank 0: regular palette, may be darkened for room transition
    // pal bank 1: dynamically changing rainbow palette (TODO)
    for (int i = 0; i < 16; ++i)
        pal_obj_bank[0][i] = gfx_palette[i];
}

void gfx_set_palette_multiplied(FIXED factor)
{
    if      (factor < 0)       factor = 0;
    else if (factor > FIX_ONE) factor = FIX_ONE;

    u16 new_palette[16];
    // const FIXED scale_factor = TO_FIXED(256.0 / 31.0) + 1;

    for (int i = 1; i < 16; ++i)
    {
        int color = gfx_palette[i];
        FIXED r = (2 * FIX_ONE) * (color & 0x1F);
        FIXED g = (2 * FIX_ONE) * ((color >> 5) & 0x1F);
        FIXED b = (2 * FIX_ONE) * ((color >> 10) & 0x1F);
        r = fxmul(r, factor);
        g = fxmul(g, factor);
        b = fxmul(b, factor);

        FIXED min_dist_sq = INT32_MAX;
        int color_index = 0;

        for (int j = 0; j < 16; ++j)
        {
            int ocolor = gfx_palette[j];
            FIXED or = (2 * FIX_ONE) * (ocolor & 0x1F);
            FIXED og = (2 * FIX_ONE) * ((ocolor >> 5) & 0x1F);
            FIXED ob = (2 * FIX_ONE) * ((ocolor >> 10) & 0x1F);

            FIXED dr = or - r;
            FIXED dg = og - g;
            FIXED db = ob - b;

            FIXED dist_sq = fxmul(dr, dr) + fxmul(dg, dg) + fxmul(db, db);
            if (dist_sq < min_dist_sq)
            {
                color_index = j;
                min_dist_sq = dist_sq;
            }
        }

        new_palette[i] = gfx_palette[color_index];
    }

    for (int i = 1; i < 16; ++i)
        pal_bg_mem[i] = new_palette[i];

    for (int i = 1; i < 16; ++i)
        pal_obj_bank[0][i] = new_palette[i];
}

// not really a blit because it ORs rather than sets but good enough!
static inline void gfx_text_bmap_blit_pixel(uint x, uint y, uint pixel)
{
    // LOG_DBG("(%i, %i) -> tile %i row %i shift %i", x, y, y & 7, (y / 8) * GFX_TEXT_BMP_COLS + (x / 8), ((x & 7) * 4));
    if (pixel & 0xF)
    {
        // divide all by 8?
        u32 *row = &GFX_TEXT_BMP_VRAM[((y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3))].data[y & 7];
        uint shf = ((x & 7) << 2);
        *row = (*row & ~(0xF << shf)) | ((pixel & 0xF) << shf);
    }
}

static void blit_tile(int x, int y, const TILE4 *src_tile)
{
    const int dx = x;
    int dy = y;
    for (int r = 0; r < 8; ++r)
    {
        u32 src_row = src_tile->data[r];

        u32 *row = &GFX_TEXT_BMP_VRAM[((dy >> 3) * GFX_TEXT_BMP_COLS + (dx >> 3))].data[dy & 7];
        int shf = (dx & 7) << 2;
        *row |= src_row << shf;

        if (shf) // shf > 0
        {
            row = &GFX_TEXT_BMP_VRAM[((dy >> 3) * GFX_TEXT_BMP_COLS + (dx >> 3) + 1)].data[dy & 7];
            shf = 32 - shf;
            *row |= src_row >> shf;
        }

        ++dy;
    }
}

void gfx_text_bmap_print(int x, int y, const char *text)
{
    const TILE4 *const text_data = (const TILE4 *)font_sprdb_gfxTiles;

    for (; *text != '\0'; ++text)
    {
        char ch = *text;
        int id;
        switch (ch)
        {
        case ' ':
            id = TEXT_CHAR_ID(0);
            break;

        case '.':
            id = TEXT_CHAR_ID(26);
            break;

        case ',':
            id = TEXT_CHAR_ID(27);
            break;

        case '!':
            id = TEXT_CHAR_ID(28);
            break;

        case '?':
            id = TEXT_CHAR_ID(29);
            break;

        case '-':
            id = TEXT_CHAR_ID(30);
            break;

        case '_':
            id = TEXT_CHAR_ID(31);
            break;

        case ':':
            id = TEXT_CHAR_ID(32);
            break;
        
        case '*': // not actually the asterisk character but whatever. sure.
            id = TEXT_CHAR_ID(33);
            break;

        // assume [a-zA-Z0-9]
        default:
            if (ch >= '0' && ch <= '9')
                id = TEXT_CHAR_ID(ch - '0');
            else
                // assume it's a letter
                id = TEXT_CHAR_ID(toupper(ch) - 'A' + 1);
        }

        blit_tile(x , y, &text_data[id]);
        blit_tile(x + 8, y, &text_data[id + 1]);
        blit_tile(x, y + 8, &text_data[id + 2]);
        blit_tile(x + 8, y + 8, &text_data[id + 3]);
        x += 12;

        // int dst_y = y;
        // const TILE4 *src_tile = text_data + id;
        // for (int r = 0; r < 8; ++r)
        // {
        //     u32 src_row = src_tile->data[r];
        //     int dst_x = x;
        //     for (int c = 0; c < 24; c += 4)
        //     {
        //         gfx_text_bmap_blit_pixel(dst_x, dst_y, (src_row >> c) & 0xF);
        //         ++dst_x;
        //     }
        //     ++dst_y;
        // }

        // x += 6;
    }
}