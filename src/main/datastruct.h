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

#ifndef DATASTRUCT_H
#define DATASTRUCT_H

#include <tonc_types.h>
#include <stddef.h>

#define DYNARR_PARAM(type, list)                                               \
    type *list, size_t *list##_size, size_t *list##_capacity
#define DYNARR(list) list, &list##_size, &list##_capacity

#define DYNARR_INSERT_SHIFT(list, size, idx) do {  \
    for (int _i = (size)++; _i > idx; --_i)  \
        (list)[_i] = (list)[_i - 1];  \
    } while (false);

#define DYNARR_REMOVE(list, size, idx) do {  \
    --(size);  \
    for (int _i = (idx); _i < (size); ++_i)  \
        (list)[_i] = (list)[_i+1];  \
    } while (false);

typedef struct pqueue_entry
{
    int priority;
    void *data;
} pqueue_entry_s;

// max priority queue

bool pqueue_enqueue(pqueue_entry_s *queue, size_t *queue_size,
                    size_t queue_capacity, void *data, int priority);
void* pqueue_dequeue(pqueue_entry_s *queue, size_t *queue_size);

#endif