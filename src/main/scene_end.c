#include "game.h"
#include "gfx.h"
#include "menu.h"
#include "scenes.h"
#include "printf.h"

#define ARRLEN(arr) (sizeof(arr) / sizeof(*arr))

static const char *menu_options[] = { "YAY!" };

struct scene_state
{
    menu_s menu;
} static state EWRAM_BSS;

static void scene_load(uintptr_t init_data)
{
    state = (struct scene_state){0};

    char str1[32];
    char str2[32];

    snprintf(str1, 32, "Red orbs: %u/%u",
             (int) g_game.collected_rorbs, GAME_MAX_RORBS);
    snprintf(str2, 32, "Secret orbs: %u/%u",
             (int) g_game.collected_borbs, GAME_MAX_BORBS);

    const char *lines[] = {
        "You beat the game!",
        str1, str2
    };
    
    int yp;
    menu_render_page("Congratulations!", lines, ARRLEN(lines), &yp);

    state.menu = (menu_s)
    {
        .selection_labels = menu_options,
        .selection_count = ARRLEN(menu_options),
        .origin_x = TEXT_CENTER_X_OFS(" YAY!", 8),
        .origin_y = yp,
    };

    menu_show(&state.menu);
}

static void scene_unload(void)
{
    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
    gfx_text_bmap_dst_clear(0, SCREEN_HEIGHT_T);
}

static void scene_frame(void)
{
    int res;
    switch (menu_update(&state.menu, &res))
    {
    case MENU_STATUS_SELECT:
        scenemgr_change(&scene_desc_menu, 0);
        break;

    default: break;
    }
}

const scene_desc_s scene_desc_end = {
    .load = scene_load,
    .unload = scene_unload,
    .frame = scene_frame
};