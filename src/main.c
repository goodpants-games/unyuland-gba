#include <tonc.h>
#include <game_sprdb.h>
#include <tileset_gfx.h>
#include <assert.h>
#include <ctype.h>
#include <maxmod.h>
#include "soundbank.h"
#include "soundbank_bin.h"

#include <world.h>
#include "game.h"
#include "gfx.h"
#include "log.h"
#include "menu.h"

// #define MAIN_PROFILE

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

static void text_test(void)
{
    gfx_text_bmap_dst_assign(0, 6, 0, 2);

    u32 black8[8] = {0x11111111, 0x11111111, 0x11111111, 0x11111111,
                    0x11111111, 0x11111111, 0x11111111, 0x11111111};
    u32 black2[8] = {0x11111111, 0x11111111, 0x00000000, 0x00000000,
                    0x00000000, 0x00000000, 0x00000000, 0x00000000};
    
    gfx_text_bmap_fill(0, 0, 30, 4, black8);
    gfx_text_bmap_fill(0, 4, 30, 1, black2);

    gfx_text_bmap_print(0, 0, "Hello, world!", TEXT_COLOR_WHITE);
    gfx_text_bmap_print(0, 12, "lorem ipsum dolor", TEXT_COLOR_WHITE);
    gfx_text_bmap_print(2, 24, "sit amet", TEXT_COLOR_WHITE);
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

static void setup_game_hud(void)
{
    // set up pause menu display
    gfx_text_bmap_dst_assign(0, 8, 0, 2);

    // set up HUD display
    gfx_text_bmap_dst_assign(18, 2, HUD_ROW_ORIGIN, 3);

    clear_game_hud();

    gfx_sprdb_s sprdb = gfx_get_sprdb((const gfx_root_header_s *)game_sprdb_data);
    gfx_draw_sprite_state_s state = (gfx_draw_sprite_state_s)
    {
        .sprdb = &sprdb,
        .dst_obj = &gfx_oam_buffer[0],
        .dst_obj_count = 16,
        .a1 = ATTR2_PRIO(0),
    };
    
    gfx_draw_sprite(&state, SPRID_GAME_UI_ICONS, 0, 0, SCREEN_HEIGHT - 10);
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
    mmSetModuleVolume((int)(1024 * 0.25));
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

__attribute__((section(".ewram")))
static u8 mm_memory[8 * (MM_SIZEOF_MODCH
                         + MM_SIZEOF_ACTCH
                         + MM_SIZEOF_MIXCH)
                         + MM_MIXLEN_21KHZ];

// Mixing buffer (globals should go in IWRAM)
// Mixing buffer SHOULD be in IWRAM, otherwise the CPU load
// will _drastially_ increase
__attribute((aligned(4)))
static u8 mm_mixing_buf[MM_MIXLEN_21KHZ];

int main(void)
{
    LOG_INIT();
    LOG_DBG("Hello, world!");

    irq_init(NULL);
    irq_add(II_VBLANK, mmVBlank);

    gfx_init();

    memcpy32(&tile_mem[0][0] + GFX_CHAR_GAME_TILESET + 2, tileset_gfxTiles,
             tileset_gfxTilesLen / sizeof(u32));
    memcpy32(tile_mem_obj[0][0].data, game_sprdb_gfxTiles, game_sprdb_gfxTilesLen / sizeof(u32));

    const map_header_s *map = world_rooms[0];
    gfx_load_map(map);
    game_init();

    game_load_room(map);
    LOG_DBG("room pos: %i %i", (int) map->px, (int) map->py);

    if (false)
    {
        entity_s *e = entity_alloc();
        e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
        e->pos.x = int2fx(32);
        e->pos.y = int2fx(32);
        e->col.w = 6;
        e->col.h = 8;
        e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
        e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
        e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
        e->sprite.ox = -1;
        e->sprite.oy = -8;
        e->mass = 2;
    }

    if (false)
    {
        entity_s *e = entity_alloc();
        e->flags |= ENTITY_FLAG_ENABLED | ENTITY_FLAG_COLLIDE | ENTITY_FLAG_MOVING;
        e->pos.x = int2fx(32);
        e->pos.y = int2fx(64);
        e->col.w = 6;
        e->col.h = 8;
        e->actor.move_speed = (FIXED)(FIX_SCALE * 1);
        e->actor.move_accel = (FIXED)(FIX_SCALE / 8);
        e->actor.jump_velocity = (FIXED)(FIX_SCALE * 2.0);
        e->sprite.ox = -1;
        e->sprite.oy = -8;
        e->mass = 4;
    }

    (void)text_test;

    mmInit(&(mm_gba_system)
    {
        .mixing_mode = MM_MIX_21KHZ,
        .mod_channel_count = 8,
        .mix_channel_count = 8,
        .module_channels   = (mm_addr)(mm_memory+0),
        .active_channels   = (mm_addr)(mm_memory+(8*MM_SIZEOF_MODCH)),
        .mixing_channels   = (mm_addr)(mm_memory+(8*(MM_SIZEOF_MODCH
                                                     + MM_SIZEOF_ACTCH))),
        .mixing_memory     = (mm_addr)mm_mixing_buf,
        .wave_memory       = (mm_addr)(mm_memory+(8*(MM_SIZEOF_MODCH
                                                     + MM_SIZEOF_ACTCH
                                                     + MM_SIZEOF_MIXCH))),
        .soundbank         = (mm_addr)soundbank_bin
    });

    setup_game_hud();
    mmStart(MOD_TESTMOD, MM_PLAY_LOOP);
    mmSetModuleVolume((int)(1024 * 0.25));

    while (true)
    {
        #ifdef MAIN_PROFILE
        uint frame_len = 0;
        profile_start();
        #endif

        // screen_print(&se_mat[GFX_BG0_INDEX][18][0], "Hello, world!");

        key_poll();

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

            static int last_player_ammo = -1;
            if (g_game.player_ammo != last_player_ammo)
            {
                last_player_ammo = g_game.player_ammo;
                LOG_DBG("print!!!");
                
                char buf[8];
                int_to_str(g_game.player_ammo, buf);
                gfx_text_bmap_print(12, HUD_Y_ORIGIN, "\x7F\x7F\x7F", TEXT_COLOR_BLACK);
                gfx_text_bmap_print(12, HUD_Y_ORIGIN, buf, TEXT_COLOR_WHITE);
            }
        }
        else
        {
            update_pause_menu();
        }

        game_render();
        
        #ifdef MAIN_PROFILE
        frame_len = profile_stop();
        LOG_DBG("frame usage: %.1f%%", (float)frame_len / 280896.f * 100.f);
        #endif

        mmFrame();

        VBlankIntrWait();
        gfx_new_frame();
    }


    return 0;
}
