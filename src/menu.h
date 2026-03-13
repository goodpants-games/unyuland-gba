#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <tonc_types.h>

typedef enum menu_status
{
    MENU_STATUS_NORMAL,
    MENU_STATUS_SELECT,
    MENU_STATUS_BACK,
}
menu_status_e;

typedef struct menu
{
    int selection_count;
    const char *const *selection_labels;

    int origin_x;
    int origin_y;
    int selected;
    int timer;

    bool no_back;
    bool centered;
}
menu_s;

int menu_calc_max_width(const char *const items[], uint item_count);
void menu_show(menu_s *menu);
menu_status_e menu_update(menu_s *menu, int *result);

#endif