#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <tonc_core.h>
#include <tonc_types.h>
#include <stddef.h>
#include "mgba.h"

typedef struct map_header
{
    u16 width;
    u16 height;
    u16 object_count;
    u16 _pad0;
} map_header_s;

extern const map_header_s *const maps[8];

INLINE const u8* map_collision_data(const map_header_s *header)
{
    return (const u8 *)(header + 1);
}

INLINE uint map_collision_data_size(const map_header_s *header)
{
    return ((uint)header->width * (uint)header->height + 4 - 1) / 4;
}

INLINE const u16* map_graphics_data(const map_header_s *header)
{
    const u8 *data = (const u8 *)header;
    uintptr_t ptr = ((uintptr_t) data + sizeof(map_header_s) + map_collision_data_size(header));
    return (const u16 *)(uintptr_t)align(ptr, sizeof(uintptr_t));
}

INLINE int map_collision_get(const u8 *data, uint pitch, uint x, uint y)
{
    uint i = y * pitch + x;
    int cell = (data[i >> 2] >> ((i & 0x3) << 1)) & 0x3;
    // int cell = (data[i / 4] >> ((i % 4) * 2)) & 0x3;
    return cell;
}

#endif