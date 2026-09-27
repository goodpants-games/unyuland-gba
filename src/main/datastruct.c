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

#include <log.h>
#include "datastruct.h"

bool pqueue_enqueue(pqueue_entry_s *queue, size_t *queue_size,
                    size_t queue_capacity, void *data, int priority)
{
    if (*queue_size == queue_capacity)
    {
        LOG_ERR("priority queue is full!");
        return false;
    }

    size_t idx = *queue_size;
    queue[(*queue_size)++] = (pqueue_entry_s) {
        .priority = priority,
        .data = data
    };

    // heap shift up
    while (idx > 0)
    {
        size_t pidx = (idx - 1) / 2;
        if (queue[pidx].priority <= queue[idx].priority)
        {
            pqueue_entry_s temp = queue[pidx];
            queue[pidx] = queue[idx];
            queue[idx] = temp;

            idx = pidx;
        } else break;
    }

    return true;
}

void* pqueue_dequeue(pqueue_entry_s *queue, size_t *queue_size)
{
    if (*queue_size == 0) return NULL;
    void *ret = queue[0].data;

    // swap root with last node
    queue[0] = queue[*queue_size - 1];
    size_t qsz = --(*queue_size);

    // heap shift down
    size_t idx = 0;
    while (true)
    {
        size_t ci1 = idx * 2 + 1;
        size_t ci2 = idx * 2 + 2;

        // no child nodes, done
        if (ci1 >= qsz) break;

        // only left child node
        if (ci2 >= qsz)
        {
            if (queue[idx].priority <= queue[ci1].priority)
            {
                pqueue_entry_s temp = queue[idx];
                queue[idx] = queue[ci1];
                queue[ci1] = temp;
            }
            
            // this means that the parent node must be on the second-to-last
            // layer. therefore, recursion should end here.
            break;
        }

        // both left and right child nodes
        if (queue[idx].priority > queue[ci1].priority &&
            queue[idx].priority > queue[ci2].priority)
        {
            break;
        }

        if (queue[ci1].priority > queue[ci2].priority)
        {
            // swap with left
            pqueue_entry_s temp = queue[idx];
            queue[idx] = queue[ci1];
            queue[ci1] = temp;
            idx = ci1;
        }
        else
        {
            // swap with right
            pqueue_entry_s temp = queue[idx];
            queue[idx] = queue[ci2];
            queue[ci2] = temp;
            idx = ci2;
        }
    }

    return ret;
}