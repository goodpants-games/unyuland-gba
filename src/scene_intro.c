#include <string.h>
#include <tonc.h>
#include <dialogue.h>

#include <intro1_gfx.h>
#include <intro2_gfx.h>
#include <intro3_gfx.h>

#include "gfx.h"
#include "scenes.h"

#define TEXT_ROW_COUNT 9

struct scene_state
{
    const char *chat_ptr;
    bool page_done;
    u8 wait_timer;
    u8 char_ypos;
    u8 char_xpos;
    u8 line;
    u8 scroll_ticks;
    u8 page_idx;
} static state EWRAM_BSS;

static bool display_page(void)
{
    if (*state.chat_ptr == 0) return true;

    char line_buf[18];
    char *line_buf_write = line_buf;
    uint text_y = 4;

    for (; *state.chat_ptr != '\f'; ++state.chat_ptr)
    {
        char ch = *state.chat_ptr;
        if (ch == '\n')
        {
            *(line_buf_write++) = 0;
            gfx_text_bmap_print(8, text_y, line_buf, TEXT_COLOR_WHITE);
            text_y += 12;

            line_buf_write = line_buf;
        }
        else
        {
            *(line_buf_write++) = ch;
        }
    }

    *line_buf_write = 0;
    gfx_text_bmap_print(8, text_y, line_buf, TEXT_COLOR_WHITE);

    ++state.chat_ptr;
    return false;
}

static void update_image(void)
{
    const TILE *tile_data;
    const SCR_ENTRY *map_data;
    size_t tile_sz, map_sz;

#define CASE(idx, name) \
    case (idx): \
        tile_data = (const TILE *)name##_gfxTiles; \
        tile_sz = name##_gfxTilesLen; \
        map_data = (const SCR_ENTRY *)name##_gfxMap; \
        map_sz = name##_gfxMapLen; \
        break;

    switch (state.page_idx)
    {
        CASE(0, intro1)
        CASE(3, intro2)
        CASE(6, intro3)

        default: return;
    }

    gfx_queue_memcpy(&tile_mem[0], tile_data, tile_sz);
    gfx_queue_memcpy(&se_mem[GFX_BG1_INDEX][0], map_data, map_sz);

#undef CASE

}

static void scene_load(uintptr_t init_data)
{
    state = (struct scene_state){0};

    gfx_ctl.win[0].enabled = true;
    gfx_ctl.bg[0].enable_win_in[0] = true;
    gfx_ctl.bg[1].enable_win_out = true;

    gfx_ctl.win[0].x0 = 0;
    gfx_ctl.win[0].x1 = SCREEN_WIDTH;
    gfx_ctl.win[0].y0 = 13 * 8 + 4;
    gfx_ctl.win[0].y1 = SCREEN_HEIGHT - 4;

    gfx_ctl.bg[0].offset_x = -8;
    gfx_ctl.bg[0].offset_y = -4;

    gfx_queue_memset(&se_mem[GFX_BG1_INDEX][0], 0, sizeof(SCREENBLOCK));

    gfx_text_bmap_dst_assign(13, TEXT_ROW_COUNT, 0, 2);

    const char *chat = dlg_get_chat_by_name("intro");
    if (!chat)
    {
        LOG_DBG("could not find chat id 'intro'!");
        return;
    }

    state.chat_ptr = chat;

    update_image();
}

static void scene_unload(void)
{
    gfx_ctl.win[0].enabled = false;
    gfx_ctl.bg[0].offset_x = 0;
    gfx_ctl.bg[0].offset_y = 0;

    gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, GFX_TEXT_BMP_ROWS);
    gfx_text_bmap_dst_clear(0, SCREEN_HEIGHT_T);
}

static void scene_frame(void)
{
    if (!state.page_done)
    {
        if (state.scroll_ticks)
        {
            if (state.scroll_ticks & 1)
                gfx_ctl.bg[0].offset_y += 2;

            --state.scroll_ticks;
        }
        else if (state.wait_timer-- == 0)
        {
            char cur_ch = *(state.chat_ptr++);
            if (cur_ch == '\f')
            {
                state.page_done = true;
            }
            else if (cur_ch == '\n')
            {
                state.char_xpos = 0;
                state.char_ypos += 12;
                ++state.line;

                char fill_str[18];
                memset(fill_str, '\x7F', 17);
                fill_str[17] = 0;

                gfx_text_bmap_print(0, state.char_ypos, fill_str, TEXT_COLOR_BLACK);
                if (state.line >= 4)
                {
                    state.scroll_ticks = 12;
                }
            }
            else
            {
                char str[2];
                str[0] = cur_ch;
                str[1] = 0;

                gfx_text_bmap_print(state.char_xpos, state.char_ypos,
                                    str, TEXT_COLOR_WHITE);
                state.char_xpos += 12;
            }
            
            state.wait_timer = 1;
        }
    }
    else if (key_hit(KEY_A | KEY_B))
    {
        if (*state.chat_ptr == 0)
        {
            scenemgr_change(&scene_desc_game, 0);
            return;
        }

        gfx_text_bmap_clear(0, 0, GFX_TEXT_BMP_COLS, TEXT_ROW_COUNT);
        ++state.page_idx;
        state.char_xpos = 0;
        state.char_ypos = 0;
        state.page_done = false;
        state.wait_timer = 0;
        state.line = 0;
        gfx_ctl.bg[0].offset_y = -4;

        update_image();
    }

    if (key_hit(KEY_START))
    {
        scenemgr_change(&scene_desc_game, 0);
        return;
    }
}

const scene_desc_s scene_desc_intro = {
    .load = scene_load,
    .unload = scene_unload,
    .frame = scene_frame
};