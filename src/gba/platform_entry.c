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

#include <tonc.h>
#include <psg_ctl.h>

void platform_app_init(void);
void platform_app_frame(void);

int main()
{
    platform_app_init();

    while (true)
    {
        VBlankIntrWait();
        psg_frame_start();
        platform_app_frame();
    }

    return 0;
}