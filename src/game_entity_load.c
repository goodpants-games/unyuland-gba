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

static int parse_gun_enemy_flags(const entity_load_s *load_data)
{
    const char *str;
    if (!get_property_string(load_data, "fire_dir", &str))
        return GUN_ENEMY_DIRFLAG_ALL;

    int flags = 0;
    for (; *str != 0; ++str)
    {
        switch (*str)
        {
        case 'L':
            flags |= GUN_ENEMY_DIRFLAG_L;
            break;
        case 'T':
            switch (*(str + 1))
            {
            case 'r':
                flags |= GUN_ENEMY_DIRFLAG_TR;
                ++str;
                break;
            case 'l':
                flags |= GUN_ENEMY_DIRFLAG_TL;
                ++str;
                break;
            default:
                flags |= GUN_ENEMY_DIRFLAG_T;
                break;
            }
            flags |= GUN_ENEMY_DIRFLAG_T;
            break;
        case 'R':
            flags |= GUN_ENEMY_DIRFLAG_R;
            break;
        }
    }

    return flags;
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
        FIXED max_dist;
        if (!get_property_decimal(load_data, "max_dist", &max_dist))
            max_dist = 0;

        entity_crawler_init(ent, load_data->x, load_data->y,
                            max_dist * WORLD_TILE_SIZE);
    }
    STR_CASE("gun_enemy")
    {
        entity_gun_enemy_init(ent, load_data->x, load_data->y, false,
                              parse_gun_enemy_flags(load_data));
    }
    STR_CASE("ceil_gun_enemy")
    {
        
        entity_gun_enemy_init(ent, load_data->x, load_data->y, true,
                              parse_gun_enemy_flags(load_data));
    }
    STR_CASE("ice_block")
    {
        entity_ice_block_init(ent, load_data->x, load_data->y);
    }
    STR_CASE("spring")
    {
        entity_spring_init(ent, load_data->x, load_data->y, false);
    }
    STR_CASE("super_spring")
    {
        entity_spring_init(ent, load_data->x, load_data->y, true);
    }
    STR_CASE("home")
    {
        entity_home_init(ent, load_data->x, load_data->y);
    }
    STR_CASE("sign")
    {
        entity_sign_init(ent, load_data->x, load_data->y, NULL, false);
    }
    STR_CASE("hint_sign")
    {
        entity_sign_init(ent, load_data->x, load_data->y, NULL, true);
    }
    STR_CASE("water_tank")
    {
        entity_water_tank_init(ent, load_data->x, load_data->y);
    }
    STR_CASE("static_fragile_block")
    {
        entity_fragile_block_init(ent, load_data->x, load_data->y);
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