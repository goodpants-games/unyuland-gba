#include "game.h"
#include <string.h>

#include "log.h"

#define STR_CASE(case) else if (!strcmp(__match, (case)))
#define STR_CASE_START(match) { const char *__match = (match); if (false) {}
#define STR_CASE_FALLBACK else
#define STR_CASE_END }

bool get_property_string(const entity_load_s *load_data, const char *name,
                         const char **out)
{
    entity_load_prop_s *prop = load_data->props;
    entity_load_prop_s *end_prop = load_data->props + load_data->prop_count;
    for (; prop != end_prop; ++prop)
    {
        if (!strcmp(prop->name, name) && prop->type == ELPT_STRING)
        {
            *out = (const char *)prop->data;
            return true;
        }
    }

    return false;
}

bool get_property_decimal(const entity_load_s *load_data, const char *name,
                          FIXED *out)
{
    entity_load_prop_s *prop = load_data->props;
    entity_load_prop_s *end_prop = load_data->props + load_data->prop_count;
    for (; prop != end_prop; ++prop)
    {
        if (!strcmp(prop->name, name) && prop->type == ELPT_DECIMAL)
        {
            *out = (FIXED) prop->data;
            return true;
        }
    }

    return false;
}

void game_load_entity(const entity_load_s *load_data)
{
    const char *name = load_data->name;

    entity_s *ent = entity_alloc();
    if (!ent)
        return;

    STR_CASE_START(name)
    STR_CASE("crawler")
    {
        LOG_DBG("spawn crawler at (%f, %f)", fx2float(load_data->x), fx2float(load_data->y));

        FIXED max_dist;
        if (!get_property_decimal(load_data, "max_dist", &max_dist))
            max_dist = 0;

        entity_crawler_init(ent, load_data->x, load_data->y,
                            max_dist * WORLD_TILE_SIZE);
    }
    STR_CASE_FALLBACK
    {
        LOG_DBG("unknown entity type %s", name);
        goto error;
    }
    STR_CASE_END

    return;

    error:
        entity_free(ent);
}