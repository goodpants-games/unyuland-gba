#ifndef GAME_H
#define GAME_H

#include <tonc_types.h>
#include "map_data.h"
#include <game_sprdb.h>

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
#define COLGROUP_ENTITY  2
#define COLGROUP_ALL     UINT16_MAX

#define MAX_ENTITY_COUNT 32
#define MAX_PROJECTILE_COUNT 128
#define MAX_CONTACT_COUNT 64

// not the actual tile rendered tile size, which is 16. but tiles are supposed
// to be 8x8.
#define WORLD_TILE_SIZE 8

#define PLAYER_DROPLET_TYPE_SIDE      0
#define PLAYER_DROPLET_TYPE_SIDE_SLOW 1
#define PLAYER_DROPLET_TYPE_UP        2

typedef enum entity_msgid
{
    ENTITY_MSG_INTERACT,
    ENTITY_MSG_ATTACKED
} entity_msgid_e;

struct entity;

typedef struct behavior_def
{
    void (*update)(struct entity *self);
    void (*message)(struct entity *self, entity_msgid_e msg, ...);
} behavior_def_s;

typedef struct entity {
    u32 flags;

    struct { FIXED x; FIXED y; } pos;
    struct { FIXED x; FIXED y; } vel;
    FIXED gmult;
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
        u8 graphic_id;
        u8 frame;
        u8 accum;
        u8 play;
        
        s16 ox;
        s16 oy;
    } sprite;

    s32 userdata[4];
    const behavior_def_s *behavior;
} entity_s;

extern entity_s game_entities[MAX_ENTITY_COUNT];
extern const u8 *game_room_collision;
extern int game_room_width;
extern int game_room_height;

extern int game_cam_x;
extern int game_cam_y;

entity_s* entity_alloc(void);
void entity_free(entity_s *entity);

void game_init(void);
void game_update(void);
void game_load_room(const map_header_s *map);

void game_render(int *last_obj_index);

void entity_player_droplet_init(entity_s *self, FIXED px, FIXED py,
                                int type, int dir);
extern const behavior_def_s behavior_player_droplet;

#endif