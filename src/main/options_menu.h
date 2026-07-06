#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H

#include <tonc_types.h>

typedef struct optmenu_config
{
    int region_x, region_y;
    int region_w, region_h;
} optmenu_config_s;

extern optmenu_config_s optmenu_config;

void optmenu_open(void);
void optmenu_close(void);

#endif