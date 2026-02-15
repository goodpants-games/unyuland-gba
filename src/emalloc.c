#include "emalloc.h"

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "log.h"

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t  u16;
typedef int16_t   s16;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uintptr_t usize;
typedef intptr_t  ssize;

#define MAGIC_INT 0x42636C41 // 'AlcB' in little-endian
#define NATIVE_SIZE sizeof(usize)

typedef struct block_header
{
    u32 magic;
    size_t size;
    struct block_header *prev;
    struct block_header *next;

    // make sure size of block header is a multiple of the native alignment!
    // this can be done by having the last element be a pointer or usize.
} block_header_s;

static_assert(sizeof(block_header_s) % NATIVE_SIZE == 0,
              "block_header_s size must be a multiple of native size!");

#define MEMORY_POOL_SIZE (1024 * 16) // 16 KiB

__attribute__((section(".ewram")))
static u8 memory[MEMORY_POOL_SIZE];

block_header_s *first_block = NULL;

static inline usize align(usize n, usize width)
{
    return (n + width - 1) / width * width;
}

void* emalloc(size_t sz)
{
    if (sz == 0) return NULL;
    sz = align(sz, NATIVE_SIZE);

    if (first_block == NULL)
    {
        first_block = (block_header_s *)memory;
        *first_block = (block_header_s)
        {
            .magic = MAGIC_INT,
            .size = sz,
            .prev = NULL,
            .next = NULL
        };

        return (void*)(first_block + 1);
    }

    block_header_s *block = first_block;
    while (true)
    {
        usize empty_start = (usize)(block + 1) + block->size;
        if (block->next == NULL)
        {
            usize empty_end = empty_start + sizeof(block_header_s) + sz;
            if (empty_end >= (usize) memory + sizeof(memory))
            {
                LOG_ERR("emalloc: ran out of space!");
                return NULL;
            }

            block_header_s *alloc = (block_header_s *)empty_start;
            *alloc = (block_header_s)
            {
                .magic = MAGIC_INT,
                .size = sz,
                .prev = block,
                .next = NULL
            };

            block->next = alloc;
            return (void*)(alloc + 1);
        }

        usize empty_end = (usize) block->next;
        usize empty_size = empty_end - empty_start;

        if (empty_size > sz)
        {
            block = block->next;
            continue;
        }

        block_header_s *alloc = (block_header_s *)empty_start;
        *alloc = (block_header_s)
        {
            .magic = MAGIC_INT,
            .size = sz,
            .prev = block,
            .next = block->next,
        };

        block->next->prev = alloc;
        block->next = alloc;
        return (void*)(alloc + 1);
    }
}

void efree(void *ptr)
{
    block_header_s *header = (block_header_s *)((u8 *)ptr - sizeof(block_header_s));
    if (header->magic != MAGIC_INT)
    {
        LOG_ERR("efree: pointer given is invalid");
        return;
    }

    block_header_s *prev = header->prev;
    block_header_s *next = header->next;

    header->next->prev = prev;
    header->prev->next = next;
}