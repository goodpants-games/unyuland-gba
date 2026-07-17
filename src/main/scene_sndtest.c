#include <tonc.h>
#include "scenes.h"
#include "sound.h"
#include "gfx.h"
#include "printf.h"

static int selected_sound = 0;

static const char *sound_names[SND_SOUND_COUNT] = {
    "PLAYER_JUMP",
    "PLAYER_SHOOT",
    "PLAYER_SPIT",
    "PLATFORM_PLACE",
    "PLAYER_DIE",
    "CHECKPOINT",
    "SPRING",
    "ENEMY_SPIT",
    "ENEMY_HURT",
    "ENEMY_DIE",
    "BOSS_HIT",
    "BOSS_LAND",
    "BOSS_DASH_WINDUP",
    "BOSS_DASH",
    "BOSS_JUMP_WINDUP",
    "BOSS_JUMP",
    "BOSS_DIE",
    "MENU_MOVE",
    "MENU_SELECT",
    "MENU_BACK",
};

static void update_sound_name(void)
{
    char strbuf[64];
    snprintf(strbuf, 64, "id:%i", selected_sound);

    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, 5);
    gfx_text_bmap_print(0, 0, strbuf, TEXT_COLOR_WHITE);
    gfx_text_bmap_print(0, 12, sound_names[selected_sound], TEXT_COLOR_WHITE);
}

static void scene_load(uintptr_t init_data)
{
    selected_sound = 0;

    gfx_queue_memset(&se_mem[GFX_BG1_INDEX][0], 0, sizeof(SCREENBLOCK));
    gfx_text_bmap_dst_assign(0, 5, 0, GFX_TEXTPAL_NORMAL);
    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, 5);

    update_sound_name();
}

static void scene_unload(void)
{
    
}

static void scene_frame(void)
{
    if (key_hit(KEY_START))
    {
        scenemgr_change(&scene_desc_menu, 0);
    }

    if (key_hit(KEY_RIGHT))
    {
        if (++selected_sound == SND_SOUND_COUNT)
            selected_sound = 0;
        update_sound_name();
    }

    if (key_hit(KEY_LEFT))
    {
        if (selected_sound-- == 0)
            selected_sound = SND_SOUND_COUNT - 1;
        update_sound_name();
    }

    if (key_hit(KEY_A))
        snd_play_no_overlap(selected_sound);
}

const scene_desc_s scene_desc_sndtest = {
    .load = scene_load,
    .unload = scene_unload,
    .frame = scene_frame
};