#ifndef DATASTRUCT_H
#define DATASTRUCT_H

#include <tonc_types.h>
#include <stddef.h>

#define DYNARR_PARAM(type, list)                                               \
    type *list, size_t *list##_size, size_t *list##_capacity
#define DYNARR(list) list, &list##_size, &list##_capacity

typedef struct pqueue_entry
{
    int priority;
    void *data;
} pqueue_entry_s;

// max priority queue

bool pqueue_enqueue(pqueue_entry_s *queue, size_t *queue_size,
                    size_t queue_capacity, pqueue_entry_s new_entry);
void* pqueue_dequeue(pqueue_entry_s *queue, size_t *queue_size);

#endif