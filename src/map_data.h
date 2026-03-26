#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <tonc_core.h>
#include <tonc_types.h>
#include <stddef.h>
#include "mgba.h"
#include "math_util.h"

#define MAP_GFX_FORMAT_GBA      0 // hardware GBA screen-entry format
#define MAP_GFX_FORMAT_MAPC16   1 // 16x16 tile screen-entry format used by mapc
#define MAP_GFX_FORMAT_CUSTOM16 2 // custom 16x16 tile format

typedef void (*map_write_scrblock_f)(uint map_entry, u16 *dest);

typedef struct map_header
{
    u16 width;
    u16 height;
    u8  px;
    u8  py;
    u8  gfx_format;

    // 1 byte of padding

    map_write_scrblock_f custom_scrblock_write; // used with custom16
    u32 col_data_offset;
    u32 gfx_data_offset;
    u32 ent_data_offset;
} map_header_s;

static inline const u8* map_collision_data(const map_header_s *header)
{
    return (const u8 *)((uintptr_t)header + header->col_data_offset);
}

static inline uint map_collision_data_size(const map_header_s *header)
{
    return CEIL_DIV((uint)header->width * (uint)header->height, 4);
}

static inline const u16* map_graphics_data(const map_header_s *header)
{
    return (const u16 *)((uintptr_t)header + header->gfx_data_offset);
}

static inline const u8* map_entity_data(const map_header_s *header)
{
    if (header->ent_data_offset == 0) return NULL;
    return (const u8 *)((uintptr_t)header + header->ent_data_offset);
}

static inline int map_collision_get(const u8 *data, uint pitch, uint x, uint y)
{
    uint i = y * pitch + x;
    int cell = (data[i >> 2] >> ((i & 0x3) << 1)) & 0x3;
    // int cell = (data[i / 4] >> ((i % 4) * 2)) & 0x3;
    return cell;
}

#endif