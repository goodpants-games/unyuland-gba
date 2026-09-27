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

#include <string.h>
#include <tonc.h>
#include "dialogue.h"

const char *dlg_get_chat_by_name(const char *name)
{
    if (!name) return NULL;
    
    // truncate given string to 16 characters. zero-pad extra characters with
    // NUL bytes if too short.
    char test_str[16];
    memset32(test_str, 0, sizeof(test_str) / 4);

    for (uint i = 0; i < 16; ++i)
    {
        if (!name[i]) break;
        test_str[i] = name[i];
    }
    
    for (int i = 0; i < dlg_get_root()->chat_count; ++i)
    {
        const dlg_chat_header_s *header = dlg_get_chat_headers() + i;
        if (memcmp(header->id, test_str, 16)) continue;

        return dlg_get_chat_data(i);
    }

    return NULL;
}