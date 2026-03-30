#include <stdint.h>
#include <string.h>
#include <tonc_video.h>
#include <tonc_input.h>

#include <automap_tiles_gfx.h>
#include <automap_bin.h>
#include <game_sprdb.h>

#include "automap.h"
#include "tonc_math.h"
#include "world.h"
#include "log.h"
#include "gfx.h"

#define TILE_STRIDE 100
#define HIGHLIGHT_TILE_OFFSET 48

#define ICON_MIN_X 7
#define ICON_MAX_X ((SCREEN_WIDTH / 2) - 15)
#define ICON_MIN_Y 7
#define ICON_MAX_Y ((SCREEN_HEIGHT / 2) - 15 - 5)

static void scrblock_write(uint map_entry, u16 *dest)
{
    const u32 *src32 = (const u32 *)(automap_tiles_gfxMap + (map_entry * 2));
    u32 *dest32 = (u32 *)dest; // should be aligned as dst stride is 2

    *dest32 = *src32;
    *(dest32 + 16) = *(src32 + TILE_STRIDE);
}

void automap_init(automap_s *map)
{
    memset(map->visited, 0, sizeof(map->visited));

    map->scrmap_header = (map_header_s)
    {
        .gfx_format = MAP_GFX_FORMAT_CUSTOM16,
        .width = AUTOMAP_WIDTH,
        .height = AUTOMAP_HEIGHT,
        .custom_scrblock_write = scrblock_write,
        .gfx_data_offset = offsetof(automap_s, scrmap)
                           - offsetof(automap_s, scrmap_header)
    };

    for (uint y = 0; y < AUTOMAP_HEIGHT; ++y)
    {
        bool yodd = y & 1;
        for (uint x = 0; x < AUTOMAP_WIDTH; ++x)
        {
            bool xodd = x & 1;

            u16 v;
            if (yodd & xodd)
                v = 0;
            else if (yodd)
                v = 3;
            else if (xodd)
                v = 2;
            else
                v = 1;

            map->scrmap[y][x] = v;
        }
    }

    // const u8 *am_data = (const u8 *)automap_bin;
    // const uint data_row_stride = WORLD_MATRIX_WIDTH * 2;

    // uint dst_y = AUTOMAP_MARGIN_Y;
    // for (uint y = 0; y < AUTOMAP_HEIGHT - AUTOMAP_MARGIN_Y * 2; ++y, ++dst_y)
    // {
    //     uint dst_x = AUTOMAP_MARGIN_X;
    //     for (uint x = 0; x < AUTOMAP_WIDTH - AUTOMAP_MARGIN_X * 2; ++x, ++dst_x)
    //     {
    //         u8 v = am_data[y * data_row_stride + x];
    //         if (v == 0xFF) continue;

    //         map->scrmap[dst_y][dst_x] = v;
    //     }
    // }
}

static inline void automap_clamp_spos(automap_s *map)
{
    map->sx = iclamp(map->sx, 0, int2fx(AUTOMAP_WIDTH * 8 - SCREEN_WIDTH / 2));
    map->sy = iclamp(map->sy, 0, int2fx(AUTOMAP_HEIGHT * 8 - SCREEN_HEIGHT / 2));
}

void automap_open_view(automap_s *map, int bg_idx)
{
    gfx_bg_s *view_bg = &gfx_ctl.bg[bg_idx & 0xFF];
    map->view_bg_idx = (u8) bg_idx;

    map->frame = 0;

    map->sx = int2fx(map->player_x - SCREEN_WIDTH / 4);
    map->sy = int2fx(map->player_y - SCREEN_HEIGHT / 4);
    automap_clamp_spos(map);

    view_bg->offset_x = fx2int(map->sx) * 2;
    view_bg->offset_y = fx2int(map->sy) * 2;

    gfx_ctl.bg_userpal[0][1] = GFX_PAL_DARK_BLUE;
    gfx_ctl.bg_userpal[0][2] = GFX_PAL_DARK_BLUE;
    gfx_ctl.bg_userpal[0][3] = GFX_PAL_RED;
    gfx_ctl.bg_userpal[0][4] = GFX_PAL_WHITE;

    map->is_open = true;
}

void automap_close_view(automap_s *map)
{
    map->is_open = false;
}

static void update_room_tiles(automap_s *map, const world_room_s *room,
                              int offset)
{
    const int w = (((int) room->map->width) * 8 * AUTOMAP_SCALE) / MAP_SCREEN_WIDTH;
    const int h = (((int) room->map->height) * 8 * AUTOMAP_SCALE) / MAP_SCREEN_HEIGHT;
    
    int l = ((int) room->x) * AUTOMAP_SCALE;
    int t = ((int) room->y) * AUTOMAP_SCALE;
    int r = l + w;
    int b = t + h;

    LOG_DBG("room bounds: (%i, %i), (%i, %i)", l,t, r,b);

    const u8 *am_data = (const u8 *)automap_bin;
    const uint data_row_stride = WORLD_MATRIX_WIDTH * 2;

    for (int y = t; y < b; ++y)
    {
        int dst_y = y + AUTOMAP_MARGIN_Y;
        for (int x = l; x < r; ++x)
        {
            if (!map->visited[y][x]) continue;
            
            u8 v = am_data[y * data_row_stride + x];
            if (v == 0xFF) continue;

            int dst_x = x + AUTOMAP_MARGIN_X;
            map->scrmap[dst_y][dst_x] = v + offset;
        }
    }
}

void automap_set_pos(automap_s *map, const world_room_s *room, int local_x,
                     int local_y)
{
    const int view_we = SCREEN_WIDTH / 3;
    const int view_he = SCREEN_HEIGHT / 3;

    const int room_x = (int) room->x;
    const int room_y = (int) room->y;
    const int room_w = ((int) room->map->width) * 8;
    const int room_h = ((int) room->map->height) * 8;

    // 16x16 px = 1 screen
    map->player_x =
        ((room_x * MAP_SCREEN_WIDTH + local_x)) * 16 / MAP_SCREEN_WIDTH
        + AUTOMAP_MARGIN_X * 8;
    map->player_y =
        ((room_y * MAP_SCREEN_HEIGHT + local_y)) * 16 / MAP_SCREEN_HEIGHT
        + AUTOMAP_MARGIN_Y * 8;

    int view_l = iclamp(local_x - view_we, 0, room_w);
    int view_r = iclamp(local_x + view_we, 0, room_w);
    int view_u = iclamp(local_y - view_he, 0, room_h);
    int view_d = iclamp(local_y + view_he, 0, room_h);

    int map_l = (view_l * AUTOMAP_SCALE) / MAP_SCREEN_WIDTH
                + room_x * AUTOMAP_SCALE;

    int map_r = (view_r * AUTOMAP_SCALE) / MAP_SCREEN_WIDTH
                + room_x * AUTOMAP_SCALE;

    int map_u = (view_u * AUTOMAP_SCALE) / MAP_SCREEN_HEIGHT
                + room_y * AUTOMAP_SCALE;

    int map_d = (view_d * AUTOMAP_SCALE) / MAP_SCREEN_HEIGHT
                + room_y * AUTOMAP_SCALE;

    const u8 *am_data = (const u8 *)automap_bin;
    const uint data_row_stride = WORLD_MATRIX_WIDTH * 2;

    // if room changed, update all tiles of old room to normal, and all tiles
    // in the new room to highlighted.
    const world_room_s *old_room = map->cur_room;
    if (old_room != room)
    {
        if (old_room)
        {
            LOG_DBG("room change");
            update_room_tiles(map, old_room, 0);
            update_room_tiles(map, room,     HIGHLIGHT_TILE_OFFSET);
        }

        map->cur_room = room;
    }

    for (int y = map_u; y < map_d; ++y)
    {
        int dst_y = y + AUTOMAP_MARGIN_Y;
        for (int x = map_l; x < map_r; ++x)
        {
            int dst_x = x + AUTOMAP_MARGIN_X;

            u8 v = am_data[y * data_row_stride + x];
            if (v == 0xFF) continue;

            map->scrmap[dst_y][dst_x] = v + HIGHLIGHT_TILE_OFFSET;
            map->visited[y][x] = true;
        }
    }
}

static void draw_sprite(gfx_draw_sprite_state_s *spr_state, uint spridx,
                        int x, int y)
{
    uint flags = 0;
    int dx = 0;
    int dy = 0;

    if (x > ICON_MAX_X)
    {
        x = ICON_MAX_X;
        flags |= 1;
        dx = 1;
    }

    if (y > ICON_MAX_Y)
    {
        y = ICON_MAX_Y;
        flags |= 2;
        dy = 1;
    }

    if (x < ICON_MIN_X)
    {
        x = ICON_MIN_X;
        flags |= 4;
        dx = -1;
    }

    if (y < ICON_MIN_Y)
    {
        y = ICON_MIN_Y;
        flags |= 8;
        dy = -1;
    }

    static u16 arrow_indices[] =
        {-1, 0, 3, 7, 2, -1, 6, -1, 1, 4, -1, -1, 5, -1, -1, -1};
    
    gfx_draw_sprite(spr_state, SPRID_GAME_MAP_ICONS, spridx, x * 2, y * 2);

    u16 index = arrow_indices[flags];
    if (index != UINT16_MAX)
        gfx_draw_sprite(spr_state, SPRID_GAME_MAP_ICONS, 2 + index,
                        (x + dx * 7) * 2, (y + dy * 7) * 2);
}

void automap_update_view(automap_s *map, gfx_draw_sprite_state_s *spr_state)
{
    gfx_bg_s *view_bg = &gfx_ctl.bg[map->view_bg_idx];

    const FIXED scroll_speed = FX(1);

    if (key_held(KEY_RIGHT))
        map->sx += scroll_speed;

    if (key_held(KEY_LEFT))
        map->sx -= scroll_speed;

    if (key_held(KEY_DOWN))
        map->sy += scroll_speed;

    if (key_held(KEY_UP))
        map->sy -= scroll_speed;

    automap_clamp_spos(map);

    int sx = fx2int(map->sx);
    int sy = fx2int(map->sy);

    view_bg->offset_x = sx * 2;
    view_bg->offset_y = sy * 2;

    gfx_ctl.bg_userpal[0][1] = (map->frame & 1)
                               ? GFX_PAL_DARK_BLUE : GFX_PAL_BLACK;
    gfx_ctl.bg_userpal[0][3] = (map->frame < 30)
                               ? GFX_PAL_RED : GFX_PAL_DARK_BLUE;
    
    // draw map sprites
    draw_sprite(spr_state, 1,
                map->player_x - 4 - sx, map->player_y - 4 - sy);
    // draw_sprite(spr_state, 1, map->player_x - 4 - sx, map->player_y - 4);

    if (++map->frame == 60)
        map->frame = 0;
}