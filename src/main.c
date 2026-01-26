#include <tonc.h>
#include <player_gfx.h>
#include <tileset_gfx.h>
#include <assert.h>
#include "mgba.h"

#include "map_data.h"
#include "gfx.h"

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

#define MAX_ENTITY_COUNT 64

typedef enum entity_msgid
{
    ENTITY_MSG_INTERACT,
    ENTITY_MSG_ATTACKED
} entity_msgid_e;

typedef struct entity {
    u32 flags;

    struct { FIXED x; FIXED y; } pos;
    struct { FIXED x; FIXED y; } vel;
    struct { FIXED x; FIXED y; } damp;
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

        s32 move_speed;
        s32 jump_velocity;
    } actor;

    struct {
        u8 sprite_id;
        u8 anim_id;
        u8 frame;
        u8 accum;
    } sprite;

    s32 internal[4];
    void (*update)(struct entity *self);
    void (*message)(struct entity *self, entity_msgid_e msg, ...);
} entity_s;

static_assert(sizeof(entity_s) % 4 == 0, "entity_s size must be word-aligned");
static entity_s entities[MAX_ENTITY_COUNT];

void entity_init(entity_s *self)
{
    memset32(self, 0, sizeof(entity_s) / 4);
    self->damp.x = int2fx(1);
    self->damp.y = int2fx(1);
    self->gmult = 255;
    self->mass = 2;
    self->col.group = COLGROUP_DEFAULT;
    self->col.mask = COLGROUP_ALL;
}

void update_entities(void)
{
    const FIXED gravity = float2fx(0.1f);
    const FIXED terminal_vel = int2fx(5);

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_s *entity = entities + i;
        if (!(entity->flags & ENTITY_FLAG_ENABLED)) continue;

        if (entity->flags & ENTITY_FLAG_MOVING)
        {
            FIXED g = fxmul(gravity, entity->gmult * (FIX_SCALE / 256));
            entity->vel.y += g;

            if (entity->vel.y > terminal_vel)
                entity->vel.y = terminal_vel;

            entity->pos.x += entity->vel.x;
            entity->pos.y += entity->vel.y;
        }
    }
}

int main()
{
    mgba_console_open();

    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    gfx_init();

    memcpy32(&tile_mem[0][0], tileset_gfxTiles, tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, player_gfxTiles, 32 * 8);

    gfx_load_map(1);

    OBJ_ATTR *obj = &gfx_oam_buffer[0];
    obj_set_attr(obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK(0));
    int cam_x = 0;
    int cam_y = 0;

    const u8 *map_collision = map_collision_data(gfx_loaded_map);

    for (int i = 0; i < MAX_ENTITY_COUNT; ++i)
    {
        entity_init(&entities[i]);
    }

    entity_s *player = &entities[0];
    player->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_MOVING;
    player->pos.x = int2fx(16);
    player->pos.y = int2fx(16);

    while (true)
    {
        VBlankIntrWait();
        gfx_new_frame();
        key_poll();

        update_entities();

        if (key_is_down(KEY_RIGHT))
        {
            player->pos.x += int2fx(2);

            // int col = map_collision_get(map_collision, gfx_loaded_map->width, (px + 8) / 8, py / 8);
            // if (col == 1)
            //     player->pos.x -= int2fx(2);
        }

        if (key_is_down(KEY_LEFT))
            player->pos.x -= int2fx(2);

        if (key_is_down(KEY_UP))
            player->pos.y -= int2fx(2);

        if (key_is_down(KEY_DOWN))
            player->pos.y += int2fx(2);

        int px = player->pos.x / FIX_SCALE;
        int py = player->pos.y / FIX_SCALE;
        cam_x = px;
        cam_y = py;

        int x_max = gfx_map_width * 8 - SCREEN_WIDTH / 2;
        int y_max = gfx_map_height * 8 - SCREEN_HEIGHT / 2;

        if (cam_x < 0)     cam_x = 0;
        if (cam_y < 0)     cam_y = 0;
        if (cam_x > x_max) cam_x = x_max;
        if (cam_y > y_max) cam_y = y_max;

        gfx_scroll_x = cam_x * 2;
        gfx_scroll_y = cam_y * 2;

        obj_set_pos(obj, (px - cam_x) * 2, (py - cam_y) * 2);
    }


    return 0;
}
