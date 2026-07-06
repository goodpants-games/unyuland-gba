#include <string.h>
#include <tonc.h>
#include <modplay.h>

#include <data/music.h>
#include <data/graphics/game_logo_gfx.h>

#include "platctl.h"
#include "scenes.h"
#include "menu.h"
#include "gfx.h"
#include "sound.h"

#define HUD_ROW_ORIGIN (GFX_TEXT_BMP_ROWS - 2)
#define HUD_Y_ORIGIN   (HUD_ROW_ORIGIN * 8 + 6)
#define ARRLEN(arr) (sizeof(arr) / sizeof(*arr))

#ifdef PLATFORM_PC
#define EXTRA_OPTIONS // i.e. volume, fulscr, shader(?)
#endif

static const uint volume_levels[6] = {
    (uint)(PLATCTL_VOLUME_MAX * 0.00),
    (uint)(PLATCTL_VOLUME_MAX * 0.05),
    (uint)(PLATCTL_VOLUME_MAX * 0.15),
    (uint)(PLATCTL_VOLUME_MAX * 0.50),
    (uint)(PLATCTL_VOLUME_MAX * 1.00),
    (uint)(PLATCTL_VOLUME_MAX * 3.00),
};

static const char *const main_menu_options[] =
    {"START", "CONTROLS", "OPTIONS", "CREDITS"};

static const char *const page_menu_options[] =
    {"BACK"};

static char volume_string[] = "VOLUME:\0\0\0\0\0";

static const char *const fulscr_off_string = "FULSCR:OFF";
static const char *const fulscr_on_string = "FULSCR:ON";

static const char *options_options[] = {
    "GBA COLOR:Off",
#ifdef EXTRA_OPTIONS
    fulscr_off_string,
    volume_string,
#endif
    "BACK"
};

enum
{
    MENU_MODE_MAIN,
    MENU_MODE_PAGE,
    MENU_MODE_OPTIONS,
};

enum
{
    OPTION_MENU_LCD_COLOR,
#ifdef EXTRA_OPTIONS
    OPTION_MENU_FULSCR,
    OPTION_MENU_VOLUME,
#endif
    OPTION_MENU_BACK,
};

struct
{
    menu_s menu;
    menu_s page_menu;
    int mode;
}
static state EWRAM_BSS;

static EWRAM_BSS bool option_lcd_color = false;
static EWRAM_BSS u8 option_volume = 4; // no more than 5
static EWRAM_BSS bool option_mute = false;
static EWRAM_BSS bool option_fulscr = false;

static void open_options_menu(bool clean);

static void render_page(const char *header, const char *lines[],
                        uint line_count)
{
    int yp;
    menu_render_page(header, lines, line_count, &yp);
    
    state.page_menu = (menu_s)
    {
        .selection_count = 1,
        .selection_labels = page_menu_options,

        .origin_x = TEXT_CENTER_X_OFS("BACK", 8),
        .origin_y = yp
    };

    menu_show(&state.page_menu);
    state.mode = MENU_MODE_PAGE;
}

static void update_volume_text(void)
{
    char *str = volume_string + 7;
    for (int i = 0; i < 5; ++i, ++str)
    {
        if (option_mute)
            *str = 'X';
        else if (i < option_volume)
            *str = '\x7F';
        else
            *str = '0';
    }
}

static inline void update_fulscr_text(void)
{
    options_options[OPTION_MENU_FULSCR] =
        option_fulscr ? fulscr_on_string : fulscr_off_string;
}

static void fulscr_change_watcher(bool fulscr)
{
    option_fulscr = fulscr;
    update_fulscr_text();
    open_options_menu(false);
}

static void open_options_menu(bool clean)
{
#ifdef EXTRA_OPTIONS
    if (clean)
    {
        option_fulscr = platctl_get_fullscreen();
        update_fulscr_text();
        platctl_set_fullscreen_change_watcher(fulscr_change_watcher);
    }
#endif

    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);

    int yp = MENU_CENTER_Y(1 + ARRLEN(options_options));
    gfx_text_bmap_print(text_center_x("Options"), yp,
                        "Options", TEXT_COLOR_BLUE);
    yp += 12;

    state.page_menu = (menu_s)
    {
        .selected = clean ? 0 : state.page_menu.selected,
        .selection_count = ARRLEN(options_options),
        .selection_labels = options_options,

        .origin_x = SCREEN_WIDTH / 2,
        .centered = true,
        .origin_y = yp
    };

    menu_show(&state.page_menu);
    state.mode = MENU_MODE_OPTIONS;
}

static void options_menu_update(void)
{
    // beware evil gotos. because i'm lazy as fuck.

    int res;
    switch (menu_update(&state.page_menu, &res))
    {
    case MENU_STATUS_SELECT:
        switch (res)
        {
        case OPTION_MENU_LCD_COLOR:
            if ((option_lcd_color = !option_lcd_color))
            {
                options_options[OPTION_MENU_LCD_COLOR] = "GBA Color:On";
                gfx_set_palette_mode(GFX_PAL_MODE_LCD_CORRECTED);
            }
            else
            {
                options_options[OPTION_MENU_LCD_COLOR] = "GBA Color:Off";
                gfx_set_palette_mode(GFX_PAL_MODE_NORMAL);
            }
            
            open_options_menu(false);

            break;
        
#ifdef EXTRA_OPTIONS
        case OPTION_MENU_VOLUME:
            option_mute = !option_mute;
            goto update_volume;
            break;

        case OPTION_MENU_FULSCR:
            platctl_set_fullscreen(!option_fulscr);
            // rest of the functionality should be handled by the fullscreen
            // change watcher
            break;
#endif

        // back button
        case OPTION_MENU_BACK:
            goto exit;
        }

        break;
    
    case MENU_STATUS_BACK:
        goto exit;

    default:
        if (state.page_menu.selected == OPTION_MENU_VOLUME)
        {
            if (key_hit(KEY_RIGHT))
            {
                if (option_volume != 5)
                {
                    ++option_volume;
                }
                else if (!option_mute) break;
                
                option_mute = false;
                snd_play_no_overlap(SND_ID_MENU_MOVE);
                goto update_volume;
            }
            else if (key_hit(KEY_LEFT))
            {
                if (option_volume != 0)
                {
                    --option_volume;
                }
                else if (!option_mute) break;

                option_mute = false;
                snd_play_no_overlap(SND_ID_MENU_MOVE);
                goto update_volume;
            }
        }

        break;
    }

    return;

    exit:
#ifdef EXTRA_OPTIONS
        platctl_set_fullscreen_change_watcher(NULL);
#endif

        gfx_ctl.bg[1].enabled = true;
        state.mode = MENU_MODE_MAIN;
        gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
        menu_show(&state.menu);
        return;

    update_volume:
        platctl_set_volume(option_mute ? 0 : volume_levels[option_volume]);
        update_volume_text();
        open_options_menu(false);
        return;
}

static void scene_load(uintptr_t data)
{
    gfx_ctl.bg[1].bpp = GFX_BG_4BPP;
    gfx_ctl.bg[1].char_block = 0;
    gfx_ctl.bg[1].enabled = true;
    
    state.mode = MENU_MODE_MAIN;
    state.menu = (menu_s)
    {
        .selection_count = ARRLEN(main_menu_options),
        .selection_labels = main_menu_options,

        .origin_x = (SCREEN_WIDTH - (7 * 12 + 8)) / 2,
        .origin_y = MENU_CENTER_Y(ARRLEN(main_menu_options)),
        .no_back = true,
    };

    update_volume_text();

    menu_show(&state.menu);
    gfx_text_bmap_dst_assign(SCREEN_HEIGHT_T / 2, GFX_TEXT_BMP_ROWS, 0,
                                GFX_TEXTPAL_NORMAL);

    gfx_queue_memset(se_mem[GFX_BG1_INDEX], 0, 1024 * 2);
    gfx_queue_memcpy(&tile_mem[0][0].data, game_logo_gfxTiles,
                    game_logo_gfxTilesLen);
    
    const SCR_ENTRY *src = (const SCR_ENTRY *)game_logo_gfxMap;
    uint oy = 2;
    uint ox = 4;
    for (uint y = oy; y < oy + 9; ++y)
    {
        const size_t alloc_size = sizeof(SCR_ENTRY) * 22;

        SCR_ENTRY *row = gfx_alloc_cpybuf(alloc_size);
        if (!row) DBG_CRASH();

        for (uint i = 0; i < 22; ++i)
            row[i] = *(src++);

        gfx_queue_memcpy(&se_mat[GFX_BG1_INDEX][y][ox], row, alloc_size);
        // for (uint x = ox; x < ox + 22; ++x)
        // {
        //     se_mat[GFX_BG1_INDEX][y][x] = *(src++);
        // }
    }

    mplay_start(MOD_SAC08, true);
    mplay_set_volume((int)(1024 * 0.3));
}

static void scene_unload(void)
{
    mplay_stop();
    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
    gfx_text_bmap_dst_clear(0, SCREEN_HEIGHT_T);
}

static void scene_frame(void)
{
    bool start = key_hit(KEY_START);
    
    if (state.mode == MENU_MODE_MAIN)
    {
        int res;
        switch (menu_update(&state.menu, &res))
        {
        case MENU_STATUS_SELECT:
            switch (res)
            {
            case 0:
                start = true;
                break;
            
            case 1:
                {
                    static const char *lines[] = {
                        "D-Pad    Move",
                        "A        Jump",
                        "B        Fire",
                        "L      Switch",
                        "Start   Pause",
                    };

                    render_page("Controls", lines, ARRLEN(lines));
                    break;
                }
                break;
            

            case 2:
                open_options_menu(true);
                break;
            
            case 3:
                {
                    static const char *lines[] = {
                        "pkhead: code, art,",
                        "music"
                    };

                    render_page("Credits", lines, ARRLEN(lines));
                    break;
                }
                break;
            }
        
        default: break;
        }
    }
    else if (state.mode == MENU_MODE_OPTIONS)
    {
        options_menu_update();
    }
    else if (state.mode == MENU_MODE_PAGE)
    {
        int res;
        menu_status_e stat = menu_update(&state.page_menu, &res);
        
        switch (stat)
        {
        case MENU_STATUS_SELECT:
        case MENU_STATUS_BACK:
            state.mode = MENU_MODE_MAIN;
            gfx_ctl.bg[1].enabled = true;
            gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
            gfx_text_bmap_dst_clear(SCREEN_HEIGHT_T / 4, GFX_TEXT_BMP_ROWS);
            gfx_text_bmap_dst_assign(SCREEN_HEIGHT_T / 2, GFX_TEXT_BMP_ROWS,
                                        0, GFX_TEXTPAL_NORMAL);
            menu_show(&state.menu);

        default: break;
        }
    }

    if (start)
        scenemgr_change(&scene_desc_intro, 0);

    if (key_held(KEY_R) && key_hit(KEY_SELECT))
        scenemgr_change(&scene_desc_sndtest, 0);
}

const scene_desc_s scene_desc_menu = {
    .load = scene_load,
    .unload = scene_unload,
    .frame = scene_frame
};