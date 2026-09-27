/*
Copyright (C) 2026  pkhead

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

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