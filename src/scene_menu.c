#include <tonc_input.h>
#include <tonc_video.h>
#include <maxmod.h>
#include <soundbank.h>
#include <string.h>
#include "scenes.h"
#include "menu.h"
#include "gfx.h"

#define HUD_ROW_ORIGIN (GFX_TEXT_BMP_ROWS - 2)
#define HUD_Y_ORIGIN   (HUD_ROW_ORIGIN * 8 + 6)
#define ARRLEN(arr) (sizeof(arr) / sizeof(*arr))

#define TEXT_CENTER_X_OFS(str, ofs) \
    ((SCREEN_WIDTH - ((sizeof(str) - 1) * 12) - (ofs)) / 2)
#define TEXT_CENTER_X(str, ofs) TEXT_CENTER_X_OFS(str, ofs)
#define MENU_CENTER_Y(sel_count) \
    (SCREEN_HEIGHT / 2 - 12 * (sel_count)) / 2

static const char *main_menu_options[] =
    {"START", "CONTROLS", "CREDITS"};

static const char *page_menu_options[] =
    {"BACK"};

enum
{
    MENU_MODE_MAIN,
    MENU_MODE_PAGE
};

struct
{
    menu_s menu;
    menu_s page_menu;
    int mode;
}
static state EWRAM_BSS;


static int text_center_x(const char *str)
{
    return (SCREEN_WIDTH - (strlen(str) * 12)) / 2;
}

static void render_page(const char *header, const char *lines[],
                        uint line_count)
{
    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
    gfx_text_bmap_dst_clear(SCREEN_HEIGHT_T / 2, GFX_TEXT_BMP_ROWS);
    gfx_text_bmap_dst_assign(SCREEN_HEIGHT_T / 4, GFX_TEXT_BMP_ROWS, 0, 2);

    int yp = MENU_CENTER_Y(line_count + 1);
    gfx_text_bmap_print(text_center_x(header), yp,
                        header, TEXT_COLOR_BLUE);
    yp += 12;

    for (uint i = 0; i < line_count; ++i, yp += 12)
    {
        gfx_text_bmap_print(text_center_x(lines[i]), yp, lines[i],
                            TEXT_COLOR_WHITE);
    }

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

static void scene_load(uintptr_t data)
{
    mmStart(MOD_SAC08, MM_PLAY_LOOP);
    mmSetModuleVolume((int)(1024 * 0.3));

    state.mode = MENU_MODE_MAIN;
    state.menu = (menu_s)
    {
        .selection_count = ARRLEN(main_menu_options),
        .selection_labels = main_menu_options,

        .origin_x = (SCREEN_WIDTH - (7 * 12 + 8)) / 2,
        .origin_y = MENU_CENTER_Y(ARRLEN(main_menu_options)),
        .no_back = true,
    };

    menu_show(&state.menu);
    gfx_text_bmap_dst_assign(SCREEN_HEIGHT_T / 2, GFX_TEXT_BMP_ROWS, 0, 2);
}

static void scene_unload(void)
{
    mmStop();
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
    else if (state.mode == MENU_MODE_PAGE)
    {
        int res;
        switch (menu_update(&state.page_menu, &res))
        {
        case MENU_STATUS_SELECT:
        case MENU_STATUS_BACK:
            state.mode = MENU_MODE_MAIN;
            gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
            gfx_text_bmap_dst_clear(SCREEN_HEIGHT_T / 4, GFX_TEXT_BMP_ROWS);
            gfx_text_bmap_dst_assign(SCREEN_HEIGHT_T / 2, GFX_TEXT_BMP_ROWS, 0, 2);
            menu_show(&state.menu);

        default: break;
        }
    }

    if (start)
        scenemgr_change(&scene_desc_game, 0);
}

const scene_desc_s scene_desc_menu = {
    .load = scene_load,
    .unload = scene_unload,
    .frame = scene_frame
};