#ifndef MAPC_H
#define MAPC_H

#include <tonc_core.h>
#include <tonc_types.h>
#include <stddef.h>
#include "math_util.h"

typedef struct mapc_header
{
    u16 width;
    u16 height;
    u8  bg_id; // 0: black; 1: outdoors

    // 3 bytes padding
    
    u32 col_data_offset;
    u32 gfx_data_offset;
    u32 ent_data_offset;
} mapc_header_s;

static inline const u8* mapc_collision_data(const mapc_header_s *header)
{
    return (const u8 *)((uintptr_t)header + header->col_data_offset);
}

static inline uint mapc_collision_data_size(const mapc_header_s *header)
{
    return CEIL_DIV((uint)header->width * (uint)header->height, 4);
}

static inline const u16* mapc_graphics_data(const mapc_header_s *header)
{
    return (const u16 *)((uintptr_t)header + header->gfx_data_offset);
}

static inline const u8* mapc_entity_data(const mapc_header_s *header)
{
    if (header->ent_data_offset == 0) return NULL;
    return (const u8 *)((uintptr_t)header + header->ent_data_offset);
}

static inline int mapc_collision_get(const u8 *data, uint pitch, uint x, uint y)
{
    uint i = y * pitch + x;
    int cell = (data[i >> 2] >> ((i & 0x3) << 1)) & 0x3;
    // int cell = (data[i / 4] >> ((i % 4) * 2)) & 0x3;
    return cell;
}

#endif