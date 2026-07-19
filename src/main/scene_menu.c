#include <string.h>
#include <tonc.h>
#include <modplay.h>

#include <data/music.h>
#include <data/graphics/game_logo_gfx.h>

#include "scenes.h"
#include "menu.h"
#include "gfx.h"
#include "sound.h"
#include "options_menu.h"

#define HUD_ROW_ORIGIN (GFX_TEXT_BMP_ROWS - 2)
#define HUD_Y_ORIGIN   (HUD_ROW_ORIGIN * 8 + 6)
#define ARRLEN(arr) (sizeof(arr) / sizeof(*arr))

enum
{
    MENU_MODE_MAIN,
    MENU_MODE_PAGE,
    MENU_MODE_OPTIONS,
};

static const char *const main_menu_options[] =
    {"START", "CONTROLS", "OPTIONS", "CREDITS"};

static const char *const page_menu_options[] =
    {"BACK"};

struct
{
    menu_s menu;
    menu_s page_menu;
    int mode;
}
static state EWRAM_BSS;

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

    mplay_start(MOD_TITLE, true);
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
#ifdef PLATFORM_PC
                    static const char *lines[] = {
                        "CTRL  KBD ACTION",
                        "A     Z   Jump  ",
                        "B     X   Fire  ",
                        "LB/RB C   Switch",
                        "START ESC Pause ",
                    };
#else
                    static const char *lines[] = {
                        "D-Pad    Move",
                        "A        Jump",
                        "B        Fire",
                        "L      Switch",
                        "Start   Pause",
                    };
#endif
                    render_page("Controls", lines, ARRLEN(lines));
                    break;
                }
                break;
            

            case 2:
                gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);

                optmenu_open(&(optmenu_config_s)
                {
                    .center_x = true,
                    .center_y = true,
                    .x = SCREEN_WIDTH / 2,
                    .y = SCREEN_HEIGHT / 4
                });

                state.mode = MENU_MODE_OPTIONS;
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
        if (!optmenu_update())
        {
            gfx_ctl.bg[1].enabled = true;
            state.mode = MENU_MODE_MAIN;
            gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
            menu_show(&state.menu);
        }
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