#include <tonc.h>

#include "menu.h"
#include "gfx.h"
#include "options_menu.h"

//------------------------------------------------------------------------------
// declarations
//------------------------------------------------------------------------------
#pragma region declarations

#define ARRLEN(arr) (sizeof(arr) / sizeof(*arr))

enum
{
    OPTION_MENU_LCD_COLOR,
#ifdef PLATFORM_PC
    OPTION_MENU_FULSCR,
    OPTION_MENU_VOLUME,
#endif
    OPTION_MENU_BACK,
};

#ifdef PLATFORM_PC
#include "platctl.h"

static const uint volume_levels[6] = {
    (uint)(PLATCTL_VOLUME_MAX * 0.00),
    (uint)(PLATCTL_VOLUME_MAX * 0.05),
    (uint)(PLATCTL_VOLUME_MAX * 0.15),
    (uint)(PLATCTL_VOLUME_MAX * 0.50),
    (uint)(PLATCTL_VOLUME_MAX * 1.00),
    (uint)(PLATCTL_VOLUME_MAX * 3.00),
};

static char volume_string[] = "VOLUME:\0\0\0\0\0";

static const char *const fulscr_off_string = "FULSCR:OFF";
static const char *const fulscr_on_string = "FULSCR:ON";
#endif

static const char *const main_menu_options[] =
    {"START", "CONTROLS", "OPTIONS", "CREDITS"};

static const char *const page_menu_options[] =
    {"BACK"};

static const char *options_options[] = {
    "GBA COLOR:Off",
#ifdef PLATFORM_PC
    fulscr_off_string,
    volume_string,
#endif
    "BACK"
};

static EWRAM_BSS bool option_lcd_color = false;

#ifdef PLATFORM_PC
static EWRAM_BSS u8 option_volume = 4; // no more than 5
static EWRAM_BSS bool option_mute = false;
static EWRAM_BSS bool option_fulscr = false;
#endif

EWRAM_BSS optmenu_config_s optmenu_config;
EWRAM_BSS menu_s page_menu;

static void open_options_menu(bool clean);

#pragma endregion declarations
//------------------------------------------------------------------------------

#ifdef PLATFORM_PC
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
#endif

static void open_options_menu(bool clean)
{
#ifdef PLATFORM_PC
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

    page_menu = (menu_s)
    {
        .selected = clean ? 0 : page_menu.selected,
        .selection_count = ARRLEN(options_options),
        .selection_labels = options_options,

        .origin_x = SCREEN_WIDTH / 2,
        .centered = true,
        .origin_y = yp
    };

    menu_show(&page_menu);
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
        
#ifdef PLATFORM_PC
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
#ifdef PLATFORM_PC
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
#endif
        break;
    }

    return;

    exit:
#ifdef PLATFORM_PC
        platctl_set_fullscreen_change_watcher(NULL);
#endif
        gfx_ctl.bg[1].enabled = true;
        state.mode = MENU_MODE_MAIN;
        gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
        menu_show(&state.menu);
        return;

#ifdef PLATFORM_PC
    update_volume:
        platctl_set_volume(option_mute ? 0 : volume_levels[option_volume]);
        update_volume_text();
        open_options_menu(false);
        return;
#endif
}