// platform (i.e. PC)-specific control
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

#ifndef PLATCTL_H
#define PLATCTL_H

#include <stdbool.h>

#define PLATCTL_VOLUME_MAX 1024 // (not *strictly* the max)

typedef void (*platctl_fulscr_change_watcher_f)(bool fulscr);

// "hardware" volume multiplier, in range [0, PLATCTL_VOLUME_MAX]
void platctl_set_volume(unsigned int volume);
void platctl_set_fullscreen(bool fulscr);
bool platctl_get_fullscreen(void);
void platctl_set_fullscreen_change_watcher(platctl_fulscr_change_watcher_f fun);
// int platctl_get_shader(void);
// void platctl_set_shader(int shader_index);

#endif