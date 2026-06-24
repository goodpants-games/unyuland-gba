#ifndef PLATFORM_GBA

#include <tonc.h>
#include "gfx.h"

void _gfx_text_blit_tile(uint x, uint y, const TILE4 *src_tile)
{
    const uint shf = (x & 7) << 2;
    uint ry = y & 7;
    u32 *row = &GFX_TEXT_BMP_VRAM[(y >> 3) * GFX_TEXT_BMP_COLS + (x >> 3)].data[ry];
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

// void gfx_text_bmap_fill(uint oc, uint or, uint cols, uint rows, u32 data[8])
// {
//     for (uint r = or; r < or + rows; ++r)
//     {
//         TILE *t = &GFX_TEXT_BMP_VRAM[r * GFX_TEXT_BMP_COLS + oc];
//         for (uint c = 0; c < cols; ++c, ++t)
//         {
//             for (uint i = 0; i < 8; ++i)
//                 t->data[i] = data[i];
//         }
//     }
// }

extern bool gfx_text_bmp_dirty_rows[GFX_TEXT_BMP_ROWS];

void gfx_text_bmap_fill(uint oc, uint or, uint cols, uint rows, u32 data[8])
{
    if (cols == 0 || rows == 0) return;

    u32 *tile = (u32 *)(gfx_text_bmp_buf + (GFX_TEXT_BMP_COLS * or + oc));
    u32 *tile_orig = tile;
    for (; rows != 0; --rows)
    {
        gfx_text_bmp_dirty_rows[or] = 1;
        for (uint ci = cols; ci != 0; --ci)
        {
            memcpy(tile, data, sizeof(u32) * 8);
            tile += 8;
        }

        tile_orig += GFX_TEXT_BMP_COLS;
        tile = tile_orig;
        ++or;
    }
}

#endif