#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <tonc_core.h>
#include <tonc_types.h>
#include <stddef.h>
#include "math_util.h"

typedef enum map_gfx_format
{
    MAP_GFX_FORMAT_GBA,      // hardware GBA screen-entry format
    MAP_GFX_FORMAT_MAPC16,   // 16x16 tile screen-entry format used by mapc
    MAP_GFX_FORMAT_CUSTOM16, // custom 16x16 tile format
} map_gfx_format_e;

typedef enum map_border
{
    MAP_BORDER_CLAMP,
    MAP_BORDER_WRAP,
} map_border_e;

typedef void (*map_write_scrblock_f)(uint map_entry, u16 *dest);

typedef struct map_header
{
    u16 width;
    u16 height;
    u8  gfx_format;
    u8  bg_id; // 0: black; 1: outdoors
    
    u8  border_x;
    u8  border_y;    

    // writer func: used with custom16
    // this is actually a 32-bit number representing an abstract handle to the
    // writer method, solely because of the PC port. It's expected to be a
    // u32 buut the PC port may be a 64-bit application. So. Like.
    uint custom_scrblock_write;
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