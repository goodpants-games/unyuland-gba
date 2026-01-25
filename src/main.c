#include <tonc.h>
#include <player_gfx.h>
#include <tileset_gfx.h>
#include "mgba.h"

#include "map_data.h"
#include "gfx.h"

int main()
{
    mgba_console_open();

    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    gfx_init();

    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, player_gfxTiles, 32 * 8);

    gfx_load_map(1);

    OBJ_ATTR *obj = &gfx_oam_buffer[0];
    obj_set_attr(obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK(0));

    int x = 96 * 2;
    int y = 32 * 2;

    while (true)
    {
        VBlankIntrWait();
        gfx_new_frame();
        key_poll();

        if (key_is_down(KEY_RIGHT))
            gfx_scroll_x += 4;

        if (key_is_down(KEY_LEFT))
            gfx_scroll_x -= 4;

        if (key_is_down(KEY_UP))
            gfx_scroll_y -= 4;

        if (key_is_down(KEY_DOWN))
            gfx_scroll_y += 4;

        int x_max = gfx_map_width * 16 - SCREEN_WIDTH;
        int y_max = gfx_map_height * 16 - SCREEN_HEIGHT;

        if (gfx_scroll_x < 0)     gfx_scroll_x = 0;
        if (gfx_scroll_y < 0)     gfx_scroll_y = 0;
        if (gfx_scroll_x > x_max) gfx_scroll_x = x_max;
        if (gfx_scroll_y > y_max) gfx_scroll_y = y_max;
    }


    return 0;
}
