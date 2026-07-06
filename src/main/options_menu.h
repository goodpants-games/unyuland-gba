#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H

#include <tonc_types.h>

typedef struct optmenu_config
{
    int x, y;
    bool center_x, center_y;
} optmenu_config_s;

void optmenu_open(const optmenu_config_s *config);
void optmenu_close(void);
bool optmenu_update(void); // returns true if menu is still open
uint optmenu_get_option_count(void);

#endif