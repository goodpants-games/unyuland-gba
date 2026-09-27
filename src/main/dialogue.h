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

#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <tonc_types.h>
#include <data/dlg_bin.h>

typedef struct dlg_chat_header
{
    char id[16];
    u16 offset;
} dlg_chat_header_s;

typedef struct dlg_root
{
    u16 chat_count;
    dlg_chat_header_s chat_header0;
} dlg_root_s;

static inline const dlg_root_s* dlg_get_root(void)
{
    return (const dlg_root_s *)dlg_bin;
}

static inline const dlg_chat_header_s* dlg_get_chat_headers(void)
{
    return &(dlg_get_root()->chat_header0);
}

static inline const char* dlg_get_chat_data(int idx)
{
    const dlg_chat_header_s *header_root = dlg_get_chat_headers();
    uintptr_t base = (uintptr_t)(header_root + dlg_get_root()->chat_count);
    return (const char *)(base + header_root[idx].offset);
}

const char *dlg_get_chat_by_name(const char *name);

#endif