#include "map_data.h"

#include <b01_map.h>
#include <room01_map.h>

const map_header_s *const maps[8] = {
    (const map_header_s *)b01_map,
    (const map_header_s *)room01_map,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};