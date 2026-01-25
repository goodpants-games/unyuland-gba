#include <tonc.h>
#include <player_gfx.h>
#include <tileset_gfx.h>
#include <map_bin.h>

OBJ_ATTR obj_buffer[128];

typedef struct map_header
{
    u16 width;
    u16 height;
} map_header_s;

int main()
{
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    // set up graphics state
    oam_init(obj_buffer, 128);

    pal_bg_mem[ 0] = 0x0000;
    pal_bg_mem[ 1] = 0x28a3;
    pal_bg_mem[ 2] = 0x288f;
    pal_bg_mem[ 3] = 0x2a00;
    pal_bg_mem[ 4] = 0x1955;
    pal_bg_mem[ 5] = 0x254b;
    pal_bg_mem[ 6] = 0x6318;
    pal_bg_mem[ 7] = 0x77df;
    pal_bg_mem[ 8] = 0x241f;
    pal_bg_mem[ 9] = 0x029f;
    pal_bg_mem[10] = 0x13bf;
    pal_bg_mem[11] = 0x1b80;
    pal_bg_mem[12] = 0x7ea5;
    pal_bg_mem[13] = 0x4dd0;
    pal_bg_mem[14] = 0x55df;
    pal_bg_mem[15] = 0x573f;

    pal_obj_mem[ 0] = 0x0000;
    pal_obj_mem[ 1] = 0x28a3;
    pal_obj_mem[ 2] = 0x288f;
    pal_obj_mem[ 3] = 0x2a00;
    pal_obj_mem[ 4] = 0x1955;
    pal_obj_mem[ 5] = 0x254b;
    pal_obj_mem[ 6] = 0x6318;
    pal_obj_mem[ 7] = 0x77df;
    pal_obj_mem[ 8] = 0x241f;
    pal_obj_mem[ 9] = 0x029f;
    pal_obj_mem[10] = 0x13bf;
    pal_obj_mem[11] = 0x1b80;
    pal_obj_mem[12] = 0x7ea5;
    pal_obj_mem[13] = 0x4dd0;
    pal_obj_mem[14] = 0x55df;
    pal_obj_mem[15] = 0x573f;

    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, player_gfxTiles, 32 * 8);

    const map_header_s *header = (const map_header_s *)map_bin;
    int map_width = (int) header->width;
    int map_height = (int) header->height;

    const u16 *map_data = (const u16 *)(map_bin + 4);

    SCR_ENTRY *se = se_mem[28];
    
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            int ii = y * map_width + x;
            int oi = ((y << 5) | x) << 1;

            int v = map_data[ii] & 0xFF;
            v <<= 1;
            se[oi] = (u16) v;
            se[++oi] = (u16) ++v;

            oi += 31;
            v += 31;
            se[oi] = (u16) v;
            se[++oi] = (u16) ++v;
        }
    }

    REG_BG0CNT = BG_CBB(0) | BG_SBB(28) | BG_4BPP | BG_REG_32x32;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_DISPCNT = DCNT_OBJ | DCNT_OBJ_1D | DCNT_BG0;

    OBJ_ATTR *obj = &obj_buffer[0];
    obj_set_attr(obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK(0));

    int x = 96 * 2;
    int y = 32 * 2;
    int sx = 0;
    int sy = 0;

    while (true)
    {
        VBlankIntrWait();
        obj_set_pos(obj, x >> 1, y >> 1);
        oam_copy(oam_mem, obj_buffer, 1);
        REG_BG0HOFS = sx;
        REG_BG0VOFS = sy;

        key_poll();

        if (key_is_down(KEY_RIGHT))
            sx += 4;

        if (key_is_down(KEY_LEFT))
            sx -= 4;

        if (key_is_down(KEY_UP))
            sy -= 4;

        if (key_is_down(KEY_DOWN))
            sy += 4;
    }


    return 0;
}
