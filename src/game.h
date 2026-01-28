#ifndef GAME_H
#define GAME_H

#include <tonc_types.h>
#include "map_data.h"

#define WORLD_SUBPX_SHIFT 4
#define WORLD_SUBPX_SCALE 16

#define ENTITY_FLAG_ENABLED  1
#define ENTITY_FLAG_MOVING   2
#define ENTITY_FLAG_ACTOR    4
#define ENTITY_FLAG_COLLIDE  8

#define ACTOR_FLAG_GROUNDED   1
#define ACTOR_FLAG_WALL       2
#define ACTOR_FLAG_DID_JUMP   4

#define COLGROUP_DEFAULT 1
#define COLGROUP_ALL UINT16_MAX

#define MAX_ENTITY_COUNT 32
#define MAX_PROJECTILE_COUNT 128
#define MAX_CONTACT_COUNT 64

// not the actual tile rendered tile size, which is 16. but tiles are supposed
// to be 8x8.
#define WORLD_TILE_SIZE 8

typedef enum entity_msgid
{
    ENTITY_MSG_INTERACT,
    ENTITY_MSG_ATTACKED
} entity_msgid_e;

typedef struct entity {
    u32 flags;

    struct { FIXED x; FIXED y; } pos;
    struct { FIXED x; FIXED y; } vel;
    u8 gmult; // 0 = no gravity, 255 = full gravity
    u8 mass; // 1 = 0.5 mass, 2 = 1 mass, 3 = 1.5 mass, e.t.c.
    u8 health;

    struct {
        u8 w; u8 h; // in world pixels
        u16 group; u16 mask;
    } col;

    struct {
        u8 flags;
        s8 move_x;
        s8 face_dir;
        u8 jump_trigger;

        FIXED move_speed;
        FIXED move_accel;
        FIXED jump_velocity;
    } actor;

    struct {
        u8 sprite_id;
        u8 anim_id;
        u8 frame;
        u8 accum;
        s8 ox;
        s8 oy;
    } sprite;

    s32 internal[4];
    void (*update)(struct entity *self);
    void (*message)(struct entity *self, entity_msgid_e msg, ...);
} entity_s;

extern entity_s game_entities[MAX_ENTITY_COUNT];
extern const u8 *game_room_collision;
extern int game_room_width;
extern int game_room_height;

void entity_init(entity_s *self);

void game_init(void);
void game_update(void);
void game_load_room(const map_header_s *map);

#endif