#include <limits.h>
#include <tonc.h>
#include <game_sprdb.h>
#include <tileset_gfx.h>
#include <automap_tiles_gfx.h>
#include <maxmod.h>
#include "math_util.h"
#include "soundbank.h"

#include <world.h>
#include "game.h"
#include "gfx.h"
#include "log.h"
#include "menu.h"
#include "tonc_memmap.h"
#include "tonc_video.h"
#include "scenes.h"
#include "automap.h"

#include <sky_bg1_gfx.h>

#define HUD_ROW_ORIGIN (GFX_TEXT_BMP_ROWS - 2)
#define HUD_Y_ORIGIN   (HUD_ROW_ORIGIN * 8 + 6)
#define HUD_SPRITE_BASE  0
#define HUD_SPRITE_COUNT 16
#define PAUSE_PALETTE_MUL FX(0.55)

#define PAUSE_MENU_OPTION_COUNT 4
static const char *pause_menu_options[PAUSE_MENU_OPTION_COUNT] =
    {"RESUME", "RESPAWN", "MAP", "QUIT"};

enum
{
    SUBSTATE_NORMAL,
    SUBSTATE_PAUSED,
    SUBSTATE_MAP,
};

struct scene_state
{
    uint last_player_ammo;
    uint last_rorbs;
    uint last_borbs;
    uint blink_timer;
    automap_s automap;
    FIXED map_sx;
    FIXED map_sy;

    u8 substate;
    menu_s pause_menu;
} static state EWRAM_BSS;

static void int_to_str(int n, char *buf)
{
    if (n < 0)
    {
        *(buf++) = '-';
        n = -n;
    }

    // buf: start
    // ch: end
    char *ch = buf;
    do
    {
        *(ch++) = (n % 10) + '0';
    }
    while ((n /= 10) != 0);
    *ch = '\0';

    // reverse output
    do
    {
        char tmp = *buf;
        *(buf++) = *(--ch);
        *ch = tmp;
    }
    while (ch > buf);
}

// DURING GAMEPLAY OR WHEN PAUSED:
//   (0-7)  ->(0-7): pause menu
//   (10-11)->(18->19): HUD
// DURING DIALOGUE:
//   everything: dialogue

static void clear_game_hud(void)
{
    u32 bg1[8] = {0x00000000, 0x00000000, 0x00000000, 0x00000000,
                 0x00000000, 0x00000000, 0x11111111, 0x11111111};
    u32 bg2[8] = {0x11111111, 0x11111111, 0x11111111, 0x11111111,
                    0x11111111, 0x11111111, 0x11111111, 0x11111111};
    gfx_text_bmap_fill(0, HUD_ROW_ORIGIN + 0, GFX_TEXT_BMP_COLS, 1, bg1);
    gfx_text_bmap_fill(0, HUD_ROW_ORIGIN + 1, GFX_TEXT_BMP_COLS, 1, bg2);
}

static void update_hud_sprites(uint face_frame, uint mode_frame)
{
    gfx_sprdb_s sprdb = gfx_get_sprdb((const gfx_root_header_s *)game_sprdb_bin);
    gfx_draw_sprite_state_s state = (gfx_draw_sprite_state_s)
    {
        .sprdb = &sprdb,
        .dst_obj = &gfx_oam_buffer[HUD_SPRITE_BASE],
        .dst_obj_count = HUD_SPRITE_COUNT,
        .a1 = ATTR2_PRIO(0),
        .a2 = ATTR2_PALBANK(GFX_OBJPAL_MUL),
    };

    const int ypos = SCREEN_HEIGHT - 10;
    gfx_draw_sprite(&state, SPRID_GAME_UI_ICONS, 0, 0, ypos);
    gfx_draw_sprite(&state, SPRID_GAME_UI_ICONS, 5 + face_frame, 48, ypos);
    gfx_draw_sprite(&state, SPRID_GAME_UI_ICONS, 3 + mode_frame, 62, ypos);

    gfx_draw_sprite(&state, SPRID_GAME_UI_ICONS, 1, 240 - 48, ypos);
    gfx_draw_sprite(&state, SPRID_GAME_UI_ICONS, 2, 240 - 24, ypos);
}

static void setup_game_hud(void)
{
    // set up pause menu display
    gfx_text_bmap_dst_assign(0, 9, 0, GFX_TEXTPAL_NORMAL);
    // set up HUD display
    gfx_text_bmap_dst_assign(18, 2, HUD_ROW_ORIGIN, GFX_TEXTPAL_MUL);

    clear_game_hud();
    update_hud_sprites(0, 0);
}

// TODO: maybe reorganize the code so these forward declarations are not
// necessary
static void unpause_game(void);
static void update_hud(bool force_dirty);

static void open_map(void)
{
    gfx_set_palette_multiplied(FIX_ONE);

    state.substate = SUBSTATE_MAP;
    automap_open_view(&state.automap, 1);

    gfx_ctl.bg[1].char_block = 1;
    gfx_ctl.bg[1].bpp = GFX_BG_4BPP;
    gfx_load_map(1, &state.automap.scrmap_header);

    // clear pause gui
    gfx_text_bmap_dst_clear(0, 9);

    // change bottom bar to say controls
    clear_game_hud();
    gfx_text_bmap_print(0, HUD_Y_ORIGIN, "B: BACK", TEXT_COLOR_WHITE);

    // hide all ui sprites
    OBJ_ATTR *obj;
    obj = gfx_oam_buffer + HUD_SPRITE_BASE;
    for (uint i = HUD_SPRITE_COUNT; i != 0; --i, ++obj)
        obj_hide(obj);

    // hide all game sprites
    obj = gfx_oam_buffer + GAME_OAM_START;
    for (uint i = 0; i < GAME_OAM_COUNT; ++i, ++obj)
        obj_hide(obj);
}

static void close_map(void)
{
    automap_close_view(&state.automap);
    gfx_ctl.bg[1].char_block = 0;
    gfx_ctl.bg[1].bpp = GFX_BG_8BPP;
    gfx_load_map(1, g_game.room->map);

    // reset pause menu display
    gfx_text_bmap_dst_assign(0, 9, 0, GFX_TEXTPAL_NORMAL);

    setup_game_hud();
    update_hud(true);
    game_render();
}

static void update_map(void)
{
    if (key_hit(KEY_START))
    {
        close_map();
        unpause_game();
        return;
    }

    if (key_hit(KEY_B))
    {
        close_map();
        state.substate = SUBSTATE_PAUSED;
        gfx_set_palette_multiplied(PAUSE_PALETTE_MUL);
        return;
    }

    for (uint i = 0; i < HUD_SPRITE_COUNT; ++i)
        obj_hide(&gfx_oam_buffer[HUD_SPRITE_BASE + i]);

    gfx_sprdb_s sprdb = gfx_get_sprdb((const gfx_root_header_s *)game_sprdb_bin);
    gfx_draw_sprite_state_s sprdraw_state = (gfx_draw_sprite_state_s)
    {
        .sprdb = &sprdb,
        .dst_obj = &gfx_oam_buffer[HUD_SPRITE_BASE],
        .dst_obj_count = HUD_SPRITE_COUNT,
        .a1 = 0,
        .a2 = ATTR2_PRIO(1) | ATTR2_PALBANK(GFX_OBJPAL_MUL),
    };

    automap_update_view(&state.automap, &sprdraw_state);
}

static void open_pause_menu(void)
{
    // background fill
    u32 bg_t[8] =  {0x00000000, 0x00000000, 0x11111111, 0x11111111,
                    0x11111111, 0x11111111, 0x11111111, 0x11111111};
    u32 bg_r[8] =  {0x00001111, 0x00001111, 0x00001111, 0x00001111,
                    0x00001111, 0x00001111, 0x00001111, 0x00001111};
    u32 bg_tr[8] = {0x00000000, 0x00000000, 0x00001111, 0x00001111,
                    0x00001111, 0x00001111, 0x00001111, 0x00001111};
    u32 bg_l[8] =  {0x11111100, 0x11111100, 0x11111100, 0x11111100,
                    0x11111100, 0x11111100, 0x11111100, 0x11111100};
    u32 bg_tl[8] = {0x00000000, 0x00000000, 0x11111100, 0x11111100,
                    0x11111100, 0x11111100, 0x11111100, 0x11111100};
    u32 bg_f[8] =  {0x11111111, 0x11111111, 0x11111111, 0x11111111,
                    0x11111111, 0x11111111, 0x11111111, 0x11111111};
    gfx_text_bmap_fill(1, 1, 11, 7, bg_f);
    gfx_text_bmap_fill(0, 0, 12, 1, bg_t);
    gfx_text_bmap_fill(12, 0, 1, 1, bg_tr);
    gfx_text_bmap_fill(12, 1, 1, 7, bg_r);
    gfx_text_bmap_fill(0, 1, 1, 7, bg_l);
    gfx_text_bmap_fill(0, 0, 1, 1, bg_tl);

    // print text
    gfx_text_bmap_print(4, 0 + 4, "PAUSED", TEXT_COLOR_BLUE);

    state.pause_menu.selected = 0;
    menu_show(&state.pause_menu);
}

static void close_pause_menu(void)
{
    u32 t[8] =  {0x00000000, 0x00000000, 0x00000000, 0x00000000,
                 0x00000000, 0x00000000, 0x00000000, 0x00000000};
    gfx_text_bmap_fill(0, 0, 13, 8, t);
}

static void pause_game(void)
{
    state.substate = SUBSTATE_PAUSED;
    open_pause_menu();
    mmSetModuleVolume((int)(1024 * 0.1));
    gfx_set_palette_multiplied(PAUSE_PALETTE_MUL);
}

static void unpause_game(void)
{
    state.substate = SUBSTATE_NORMAL;
    close_pause_menu();
    mmSetModuleVolume((int)(1024 * 0.3));
    gfx_set_palette_multiplied(FIX_ONE);
}

static void update_pause_menu(void)
{
    int menu_result;
    switch (menu_update(&state.pause_menu, &menu_result))
    {
        case MENU_STATUS_SELECT:
            switch (menu_result)
            {
                case 0: // resume
                    unpause_game();
                    break;
                case 1: // respawn
                    game_restore_state();
                    unpause_game();
                    break;
                case 2: // map
                    open_map();
                    break;
                case 3: // quit
                    scenemgr_change(&scene_desc_menu, 0);
                    break;
            }
            break;

        case MENU_STATUS_BACK:
            unpause_game();
            break;

        default: break;
    }
}

static void update_hud(bool force_dirty)
{
    char buf[8];

    if (force_dirty || g_game.player_ammo != state.last_player_ammo)
    {
        state.last_player_ammo = g_game.player_ammo;
        
        int_to_str(g_game.player_ammo, buf);
        gfx_text_bmap_print(12, HUD_Y_ORIGIN, "\x7F\x7F\x7F", TEXT_COLOR_BLACK);
        gfx_text_bmap_print(12, HUD_Y_ORIGIN, buf, TEXT_COLOR_WHITE);
    }

    if (force_dirty || g_game.collected_rorbs != state.last_rorbs)
    {
        state.last_rorbs = g_game.collected_rorbs;
        int_to_str(g_game.collected_rorbs, buf);

        gfx_text_bmap_print(240 - 36, HUD_Y_ORIGIN, "\x7F", TEXT_COLOR_BLACK);
        gfx_text_bmap_print(240 - 36, HUD_Y_ORIGIN, buf, TEXT_COLOR_WHITE);
    }

    if (force_dirty || g_game.collected_borbs != state.last_borbs)
    {
        state.last_borbs = g_game.collected_borbs;
        int_to_str(g_game.collected_borbs, buf);

        gfx_text_bmap_print(240 - 12, HUD_Y_ORIGIN, "\x7F", TEXT_COLOR_BLACK);
        gfx_text_bmap_print(240 - 12, HUD_Y_ORIGIN, buf, TEXT_COLOR_WHITE);
    }

    uint face_frame = 0;
    if (g_game.player_spit_mode == PLAYER_SPIT_MODE_BULLET)
        face_frame = 1;

    if (state.blink_timer <= 4)
        face_frame = 2;

    if (--state.blink_timer == 0)
        state.blink_timer = 150;

    if (g_game.player_is_dead)
        face_frame = 3;
    
    update_hud_sprites(face_frame, g_game.player_spit_mode);
}

static void scene_load(uintptr_t data)
{
    gfx_ctl.bg[1].bpp = GFX_BG_8BPP;
    gfx_ctl.bg[1].char_block = 0;
    gfx_ctl.bg[1].enabled = true;

    state = (struct scene_state)
    {
        .last_player_ammo = UINT_MAX,
        .last_rorbs = UINT_MAX,
        .last_borbs = UINT_MAX,
        .blink_timer = 150,
        .substate = SUBSTATE_NORMAL,
        .pause_menu = (menu_s)
        {
            .selection_count = PAUSE_MENU_OPTION_COUNT,
            .selection_labels = pause_menu_options,

            .origin_x = 2,
            .origin_y = 16,
        },
    };

    automap_init(&state.automap);

    const world_room_s *room = &world_rooms[0];
    gfx_load_map(1, room->map);
    gfx_ctl.bg[1].offset_x = 0;
    gfx_ctl.bg[1].offset_y = 0;
    game_init();

    gfx_ctl.bg[2].bpp = GFX_BG_4BPP;
    gfx_ctl.bg[2].char_block = 1;
    gfx_ctl.bg[2].enabled = true;

    game_load_room(room);
    game_reset_player_pos();
    setup_game_hud();

    mmStart(MOD_TESTMOD, MM_PLAY_LOOP);
    mmSetModuleVolume((int)(1024 * 0.3));

    gfx_queue_memset(&tile_mem[0][0], 0,
                     (GFX_CHAR_GAME_TILESET + 2) * sizeof(TILE) * 4);
    gfx_queue_memcpy(&tile_mem[0][0] + GFX_CHAR_GAME_TILESET + 2,
                     tileset_gfxTiles, tileset_gfxTilesLen);
    gfx_queue_memcpy(tile_mem_obj[0][0].data, game_sprdb_gfxTiles,
                     game_sprdb_gfxTilesLen);
    gfx_queue_memcpy(&tile_mem[1][0], automap_tiles_gfxTiles,
                     automap_tiles_gfxTilesLen);
    
    gfx_queue_memcpy(&se_mem[GFX_BG2_INDEX], sky_bg1_gfxMap, 32 * 32 * 2);
    gfx_queue_memcpy(&tile_mem[1][0], sky_bg1_gfxTiles, sky_bg1_gfxTilesLen);
}

static void scene_unload(void)
{
    mmStop();
    gfx_unload_map(1);
    gfx_ctl.bg[1].offset_x = 0;
    gfx_ctl.bg[1].offset_y = 0;
    gfx_text_bmap_dst_clear(0, SCREEN_HEIGHT_T);
    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
    gfx_reset_palette();
    obj_hide_multi(gfx_oam_buffer, GFX_OBJ_COUNT);
}

static void scene_frame(void)
{
    if (key_hit(KEY_START))
    {
        if (state.substate == SUBSTATE_NORMAL)
        {
            pause_game();
        }
        else if (state.substate == SUBSTATE_PAUSED)
        {
            unpause_game();
        }
    }

    switch (state.substate)
    {
    case SUBSTATE_NORMAL:
    {
        game_update();

        const entity_s *player = &g_game.entities[0];
        int px = fx2int(player->pos.x);
        int py = fx2int(player->pos.y);
        automap_set_pos(&state.automap, g_game.room, px, py);

        update_hud(false);
        game_render();
        break;
    }
    
    case SUBSTATE_PAUSED:
        update_pause_menu();
        break;

    case SUBSTATE_MAP:
        update_map();
        break;
    }
}

const scene_desc_s scene_desc_game = {
    .load = scene_load,
    .unload = scene_unload,
    .frame = scene_frame
};