#include <limits.h>
#include <tonc.h>
#include <game_sprdb.h>
#include <tileset_gfx.h>
#include <assert.h>
#include <maxmod.h>
#include "math_util.h"
#include "soundbank.h"

#include <world.h>
#include "game.h"
#include "gfx.h"
#include "log.h"
#include "menu.h"
#include "tonc_video.h"
#include "scenes.h"

#define HUD_ROW_ORIGIN (GFX_TEXT_BMP_ROWS - 2)
#define HUD_Y_ORIGIN   (HUD_ROW_ORIGIN * 8 + 6)

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
        .dst_obj = &gfx_oam_buffer[0],
        .dst_obj_count = 16,
        .a1 = ATTR2_PRIO(0),
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
    gfx_text_bmap_dst_assign(0, 9, 0, 2);

    // set up HUD display
    gfx_text_bmap_dst_assign(18, 2, HUD_ROW_ORIGIN, 3);

    clear_game_hud();
    update_hud_sprites(0, 0);
}

#define PAUSE_MENU_OPTION_COUNT 4
static const char *pause_menu_options[PAUSE_MENU_OPTION_COUNT] =
    {"RESUME", "RESPAWN", "MAP", "QUIT"};

static bool game_paused = false;

static menu_s pause_menu = (menu_s)
{
    .selection_count = PAUSE_MENU_OPTION_COUNT,
    .selection_labels = pause_menu_options,

    .origin_x = 2,
    .origin_y = 16,
};

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

    menu_show(&pause_menu);
}

static void close_pause_menu(void)
{
    u32 t[8] =  {0x00000000, 0x00000000, 0x00000000, 0x00000000,
                 0x00000000, 0x00000000, 0x00000000, 0x00000000};
    gfx_text_bmap_fill(0, 0, 13, 8, t);
}

static void pause_game(void)
{
    game_paused = true;
    open_pause_menu();
    mmSetModuleVolume((int)(1024 * 0.1));
    gfx_set_palette_multiplied(TO_FIXED(0.55));
}

static void unpause_game(void)
{
    game_paused = false;
    close_pause_menu();
    mmSetModuleVolume((int)(1024 * 0.3));
    gfx_set_palette_multiplied(FIX_ONE);
}

static void update_pause_menu(void)
{
    int menu_result;
    switch (menu_update(&pause_menu, &menu_result))
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
                    game_save_state();
                    break;
                case 3: // quit
                    SoftReset();
                    break;
            }
            break;

        case MENU_STATUS_BACK:
            unpause_game();
            break;

        default: break;
    }
}

static void update_hud(void)
{
    char buf[8];
    static uint last_player_ammo = UINT_MAX;
    static uint last_rorbs = UINT_MAX;
    static uint last_borbs = UINT_MAX;
    static uint blink_timer = 150;

    if (g_game.player_ammo != last_player_ammo)
    {
        last_player_ammo = g_game.player_ammo;
        
        int_to_str(g_game.player_ammo, buf);
        gfx_text_bmap_print(12, HUD_Y_ORIGIN, "\x7F\x7F\x7F", TEXT_COLOR_BLACK);
        gfx_text_bmap_print(12, HUD_Y_ORIGIN, buf, TEXT_COLOR_WHITE);
    }

    if (g_game.collected_rorbs != last_rorbs)
    {
        last_rorbs = g_game.collected_rorbs;
        int_to_str(g_game.collected_rorbs, buf);

        gfx_text_bmap_print(240 - 36, HUD_Y_ORIGIN, "\x7F", TEXT_COLOR_BLACK);
        gfx_text_bmap_print(240 - 36, HUD_Y_ORIGIN, buf, TEXT_COLOR_WHITE);
    }

    if (g_game.collected_borbs != last_borbs)
    {
        last_borbs = g_game.collected_borbs;
        int_to_str(g_game.collected_borbs, buf);

        gfx_text_bmap_print(240 - 12, HUD_Y_ORIGIN, "\x7F", TEXT_COLOR_BLACK);
        gfx_text_bmap_print(240 - 12, HUD_Y_ORIGIN, buf, TEXT_COLOR_WHITE);
    }

    uint face_frame = 0;
    if (g_game.player_spit_mode == PLAYER_SPIT_MODE_BULLET)
        face_frame = 1;

    if (blink_timer <= 4)
        face_frame = 2;

    if (--blink_timer == 0) blink_timer = 150;

    if (g_game.player_is_dead)
        face_frame = 3;
    
    update_hud_sprites(face_frame, g_game.player_spit_mode);
}

static void scene_load(uintptr_t data)
{
    memcpy32(&tile_mem[0][0] + GFX_CHAR_GAME_TILESET + 2, tileset_gfxTiles,
             tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, game_sprdb_gfxTiles, game_sprdb_gfxTilesLen / sizeof(u32));

    const map_header_s *map = world_rooms[0];
    gfx_load_map(map);
    game_init();

    game_load_room(map);
    LOG_DBG("room pos: %i %i", (int) map->px, (int) map->py);

    setup_game_hud();

    mmStart(MOD_TESTMOD, MM_PLAY_LOOP);
    mmSetModuleVolume((int)(1024 * 0.3));
}

static void scene_unload(void)
{
    mmStop();
}

static void scene_frame(void)
{
    if (key_hit(KEY_START))
    {
        if (!game_paused)
        {
            pause_game();
        }
        else
        {
            unpause_game();
        }
    }

    if (!game_paused)
    {
        game_update();
        update_hud();
    }
    else
    {
        update_pause_menu();
    }

    game_render();
}

const scene_desc_s scene_desc_game = {
    .load = scene_load,
    .unload = scene_unload,
    .frame = scene_frame
};