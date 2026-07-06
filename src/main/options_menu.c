#include <tonc.h>

#include "menu.h"
#include "gfx.h"
#include "options_menu.h"
#include "sound.h"

//------------------------------------------------------------------------------
// declarations
//------------------------------------------------------------------------------
#pragma region declarations

#define ARRLEN(arr) (sizeof(arr) / sizeof(*arr))
#define MAX_CHARACTERS 13

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

static const char *options_options[] = {
    "GBA COLOR:Off",
#ifdef PLATFORM_PC
    fulscr_off_string,
    volume_string,
#endif
    "BACK"
};

static char clear_string[MAX_CHARACTERS + 1];

static EWRAM_DATA bool option_lcd_color = false;

#ifdef PLATFORM_PC
static EWRAM_DATA u8 option_volume = 4; // no more than 5
static EWRAM_DATA bool option_mute = false;
static EWRAM_DATA bool option_fulscr = false;
#endif

static EWRAM_BSS menu_s page_menu;

#pragma endregion declarations
//------------------------------------------------------------------------------










//------------------------------------------------------------------------------
// functions
//------------------------------------------------------------------------------
#pragma region functions

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

    menu_draw_item(&page_menu, OPTION_MENU_FULSCR, true);
    update_fulscr_text();
    menu_draw_item(&page_menu, OPTION_MENU_FULSCR, false);
}
#endif

#pragma endregion functions
//------------------------------------------------------------------------------









//------------------------------------------------------------------------------
// public
//------------------------------------------------------------------------------
#pragma region public

void optmenu_open(const optmenu_config_s *config)
{
#ifdef PLATFORM_PC
    option_fulscr = platctl_get_fullscreen();
    platctl_set_fullscreen_change_watcher(fulscr_change_watcher);
    
    update_fulscr_text();
    update_volume_text();
#endif

    memset(clear_string, '\x7F', sizeof(clear_string));
    clear_string[sizeof(clear_string) - 1] = 0;

    int xpos = config->x;
    int ypos = config->y;

    if (config->center_y)
        ypos -= (int)(1 + ARRLEN(options_options)) * 12 / 2;

    int header_xpos = xpos;
    if (config->center_x)
        header_xpos -= strlen("Options") * 12 / 2;

    gfx_text_bmap_print(header_xpos, ypos, "Options", TEXT_COLOR_BLUE);
    ypos += 12;

    page_menu = (menu_s)
    {
        .selected = 0,
        .selection_count = ARRLEN(options_options),
        .selection_labels = options_options,

        .centered = config->center_x,
        .origin_x = xpos,
        .origin_y = ypos,
    };

    menu_show(&page_menu);
}

bool optmenu_update(void)
{
    // beware evil gotos. because i'm lazy as fuck.

    int res;
    switch (menu_update(&page_menu, &res))
    {
    case MENU_STATUS_SELECT:
        switch (res)
        {
        case OPTION_MENU_LCD_COLOR:
            menu_draw_item(&page_menu, OPTION_MENU_LCD_COLOR, true);

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
            
            menu_draw_item(&page_menu, OPTION_MENU_LCD_COLOR, false);

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
        if (page_menu.selected == OPTION_MENU_VOLUME)
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

    return true;

    exit:
#ifdef PLATFORM_PC
        platctl_set_fullscreen_change_watcher(NULL);
#endif
        return false;

#ifdef PLATFORM_PC
    update_volume:
        platctl_set_volume(option_mute ? 0 : volume_levels[option_volume]);

        menu_draw_item(&page_menu, OPTION_MENU_VOLUME, true);
        update_volume_text();
        menu_draw_item(&page_menu, OPTION_MENU_VOLUME, false);

        return true;
#endif
}

#pragma endregion public
//------------------------------------------------------------------------------