#include <tonc_input.h>
#include <string.h>
#include "menu.h"
#include "gfx.h"
#include "sound.h"

void menu_show(menu_s *menu)
{
    int yp = menu->origin_y;
    for (int i = 0; i < menu->selection_count; ++i, yp += 12)
    {
        int draw_x = menu_calc_draw_x(menu, menu->selection_labels[i]);

        bool sel = i == menu->selected;
        const text_color_e col = sel ? TEXT_COLOR_YELLOW : TEXT_COLOR_WHITE;
        gfx_text_bmap_print(draw_x + MENU_DOT_ADVANCE_X, yp,
                            menu->selection_labels[i], col);

        if (sel)
            gfx_text_bmap_print(draw_x, yp, "*", TEXT_COLOR_YELLOW);
    }

    menu->timer = 0;
}

void menu_draw_item(menu_s *menu, uint item_index, bool clear)
{
    int draw_x = menu_calc_draw_x(menu, menu->selection_labels[item_index]);
    int draw_y = menu->origin_y + item_index * 12;

    if (clear)
    {
        gfx_text_bmap_print(draw_x + MENU_DOT_ADVANCE_X, draw_y,
                            menu->selection_labels[item_index],
                            TEXT_COLOR_BLACK);    
        gfx_text_bmap_print(draw_x, draw_y, "*", TEXT_COLOR_BLACK);
    }
    else
    {
        bool sel = item_index == menu->selected;
        text_color_e col = sel ? TEXT_COLOR_YELLOW : TEXT_COLOR_WHITE;
        gfx_text_bmap_print(draw_x + MENU_DOT_ADVANCE_X, draw_y,
                            menu->selection_labels[item_index], col);
        
        if (sel)
        {
            col = menu->timer < 20 ? TEXT_COLOR_YELLOW : TEXT_COLOR_BLACK;
            gfx_text_bmap_print(draw_x, draw_y, "*", col);
        }
    }
}

menu_status_e menu_update(menu_s *menu, int *result)
{
    menu_status_e status = MENU_STATUS_NORMAL;

    int dot_x = menu_calc_draw_x(menu, menu->selection_labels[menu->selected]);
    int text_x = dot_x + MENU_DOT_ADVANCE_X;
    int text_y = menu->selected * 12 + menu->origin_y;
    
    if (key_hit(KEY_A))
    {
        *result = menu->selected;
        status = MENU_STATUS_SELECT;
        snd_play_no_overlap(SND_ID_MENU_SELECT);

        menu->timer = 0;
        menu_draw_item(menu, menu->selected, false);
        
        goto no_sync;
    }
    else if (key_hit(KEY_B) && !menu->no_back)
    {
        status = MENU_STATUS_BACK;
        snd_play_no_overlap(SND_ID_MENU_BACK);
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

        dot_x = menu_calc_draw_x(menu, menu->selection_labels[menu->selected]);
        text_x = dot_x + MENU_DOT_ADVANCE_X;

        gfx_text_bmap_print(text_x, text_y,
                            menu->selection_labels[menu->selected],
                            TEXT_COLOR_YELLOW);
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_YELLOW);

        snd_play_no_overlap(SND_ID_MENU_MOVE);
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

        dot_x = menu_calc_draw_x(menu, menu->selection_labels[menu->selected]);
        text_x = dot_x + MENU_DOT_ADVANCE_X;

        gfx_text_bmap_print(text_x, text_y,
                            menu->selection_labels[menu->selected],
                            TEXT_COLOR_YELLOW);
        gfx_text_bmap_print(dot_x, text_y, "*", TEXT_COLOR_YELLOW);

        snd_play_no_overlap(SND_ID_MENU_MOVE);
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

int menu_calc_max_width(const char *const items[], uint item_count)
{
    int max_width = 0;
    for (int i = 0; i < item_count; ++i)
    {
        int width = strlen(items[i]) * 12 + MENU_DOT_ADVANCE_X;
        if (width > max_width) max_width = width;
    }

    return max_width;
}

int menu_calc_draw_x(const menu_s *menu, const char *text)
{
    if (menu->centered)
        return menu->origin_x -
               ((strlen(text) * 12 + MENU_DOT_ADVANCE_X)) / 2;
    else
        return menu->origin_x;
}

int menu_calc_draw_y(const menu_s *menu, uint item_index)
{
    return menu->origin_y + item_index * 12;
}

void menu_render_page(const char *header, const char *lines[], uint line_count,
                      int *o_yp)
{
    gfx_ctl.bg[1].enabled = false;

    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
    gfx_text_bmap_dst_clear(SCREEN_HEIGHT_T / 2, GFX_TEXT_BMP_ROWS);
    gfx_text_bmap_dst_assign(SCREEN_HEIGHT_T / 4, GFX_TEXT_BMP_ROWS, 0,
                             GFX_TEXTPAL_NORMAL);

    int yp = MENU_CENTER_Y(line_count + 1);
    gfx_text_bmap_print(text_center_x(header, SCREEN_WIDTH), yp,
                        header, TEXT_COLOR_BLUE);
    yp += 12;

    for (uint i = 0; i < line_count; ++i, yp += 12)
    {
        gfx_text_bmap_print(text_center_x(lines[i], SCREEN_WIDTH), yp, lines[i],
                            TEXT_COLOR_WHITE);
    }

    *o_yp = yp;
}