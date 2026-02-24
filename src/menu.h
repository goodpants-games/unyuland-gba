#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

typedef enum menu_status
{
    MENU_STATUS_NORMAL,
    MENU_STATUS_SELECT
}
menu_status_e;

typedef struct menu
{
    int selection_count;
    const char **selection_labels;

    int origin_x;
    int origin_y;
    int vram_row_base;
    int vram_row_count;

    int selected;
    int timer;
}
menu_s;

void menu_show(menu_s *menu);
menu_status_e menu_update(menu_s *menu, int *result);
void menu_vram_refresh(menu_s *menu);

#endif