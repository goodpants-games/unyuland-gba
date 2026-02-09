#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <tonc_core.h>
#include <tonc_types.h>
#include <stddef.h>
#include "mgba.h"
#include "math_util.h"

typedef struct map_header
{
    u16 width;
    u16 height;
    u16 object_count;
    u8  px;
    u8  py;
} map_header_s;

INLINE const u8* map_collision_data(const map_header_s *header)
{
    return (const u8 *)(header + 1);
}

INLINE uint map_collision_data_size(const map_header_s *header)
{
    return CEIL_DIV((uint)header->width * (uint)header->height, 4);
}

INLINE const u16* map_graphics_data(const map_header_s *header)
{
    uintptr_t ptr = ((uintptr_t)(header + 1) + map_collision_data_size(header));
    return (const u16 *)(uintptr_t)align(ptr, 4);
}

INLINE int map_collision_get(const u8 *data, uint pitch, uint x, uint y)
{
    uint i = y * pitch + x;
    int cell = (data[i >> 2] >> ((i & 0x3) << 1)) & 0x3;
    // int cell = (data[i / 4] >> ((i % 4) * 2)) & 0x3;
    return cell;
}

#endif