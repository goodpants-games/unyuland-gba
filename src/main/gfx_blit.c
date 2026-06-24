#ifndef PLATFORM_GBA

#include <tonc.h>
#include "gfx.h"

extern bool gfx_text_bmp_dirty_rows[GFX_TEXT_BMP_ROWS];

void _gfx_text_blit_tile(uint x, uint y, const TILE4 *src_tile)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &gfx_text_bmp_buf[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
    const u32 *src_row = src_tile->data;
    
    if (shf)
    {
        for (int r = 0; r < 8; ++r)
        {
            u32 src = *src_row;
            *row |= src << shf;
            *(row + 8) |= src >> (32 - shf);

            ++row;
            ++src_row;
            if (++ry == 8)
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
        }
    }
    else
    {
        for (int r = 0; r < 8; ++r)
        {
            u32 src = *src_row;
            *row |= src;

            ++row;
            ++src_row;
            if (++ry == 8)
                row += (GFX_TEXT_BMP_COLS - 1) * 8;
        }
    }
}

void gfx_text_bmap_fill(uint oc, uint or, uint cols, uint rows, u32 data[8])
{
    if (cols == 0 || rows == 0) return;

    TILE *tile = gfx_text_bmp_buf + (GFX_TEXT_BMP_COLS * or + oc);
    for (; rows != 0; --rows)
    {
        gfx_text_bmp_dirty_rows[or] = 1;
        
        u32 *write = tile->data;
        for (uint ci = cols; ci != 0; --ci)
        {
            memcpy(write, data, sizeof(u32) * 8);
            write += 8;
        }

        tile += GFX_TEXT_BMP_COLS;
        ++or;
    }
}

#endif