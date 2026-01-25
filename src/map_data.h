#ifndef __map_data_h__
#define __map_data_h__

#include <tonc_types.h>

typedef struct map_header
{
    u16 width;
    u16 height;
} map_header_s;

extern const map_header_s *const maps[2];

INLINE const u16* map_graphics_data(const map_header_s *header)
{
    return (const u16 *)((byte *)header + 4);
}

#endif