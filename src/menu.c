#include <tonc_input.h>

#include "menu.h"
#include "gfx.h"

#define TEXT_OFFSET 8 // origin_x is location of dot

void menu_show(menu_s *menu)
{
    for (int i = 0, yp = menu->origin_y; i < 4; ++i, yp += 12)
    {
        bool sel = i == menu->selected;
        const text_color_e col = sel ? TEXT_COLOR_YELLOW : TEXT_COLOR_WHITE;
        gfx_text_bmap_print(menu->origin_x + TEXT_OFFSET, yp,
                            menu->selection_labels[i], col);

        if (sel)
            gfx_text_bmap_print(menu->origin_x, yp, "*", TEXT_COLOR_YELLOW);
    }

    menu->timer = 0;
}

menu_status_e menu_update(menu_s *menu, int *result)
{
    menu_status_e status = MENU_STATUS_NORMAL;

    const int dot_x = menu->origin_x;
    const int text_x = dot_x + TEXT_OFFSET;
    int text_y = menu->selected * 12 + menu->origin_y;
    
    if (key_hit(KEY_A))
    {
        *result = menu->selected;
        status = MENU_STATUS_SELECT;
        goto no_sync;
    }
    else if (key_hit(KEY_B))
    {
        status = MENU_STATUS_BACK;
        goto no_sync;
    }
    else if (key_hit(KEY_DOWN))
    {
        gfx_text_bmap_print(text_x, text_y,
                            menu->selection_labels[menu->selected],
                            TEXT_COLOR_WHITE);
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_BLACK);

        if (++menu->selected == menu->selection_count)
        {
            text_y = menu->origin_y;
            menu->selected = 0;
        }
        else
        {
            text_y += 12;
        }

        menu->timer = 0;

        gfx_text_bmap_print(text_x, text_y,
                            menu->selection_labels[menu->selected],
                            TEXT_COLOR_YELLOW);
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_YELLOW);
    }
    else if (key_hit(KEY_UP))
    {
        gfx_text_bmap_print(text_x, text_y,
                            menu->selection_labels[menu->selected],
                            TEXT_COLOR_WHITE);
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_BLACK);

        if (menu->selected == 0)
        {
            menu->selected = menu->selection_count - 1;
            text_y = (menu->selection_count - 1) * 12 + text_y;
        }
        else
        {
            --menu->selected;
            text_y -= 12;
        }

        menu->timer = 0;

        gfx_text_bmap_print(text_x, text_y,
                            menu->selection_labels[menu->selected],
                            TEXT_COLOR_YELLOW);
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_YELLOW);
    }
    else if (menu->timer == 20)
    {
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_BLACK);
    }
    else if (menu->timer == 40)
    {
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_YELLOW);
        menu->timer = 0;
    }
    else goto no_sync;    
    
    no_sync:;
    ++menu->timer;
    return status;
}