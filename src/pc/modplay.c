#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LIBXMP_STATIC

#include <modplay.h>
#include <tonc.h>
#include <libxmp-lite/xmp.h>

#include <data/music.h>
#include "../main/log.h"

extern const void *const mpt_module_banks[MODDAT_NSONGS];
extern const size_t mpt_module_sizes[MODDAT_NSONGS];

#define VOLUME_SCALE 1024

static mplay_event_handler_f s_ev_handler = NULL;
static xmp_context s_main_xmpc;
static xmp_context s_sub_xmpc;
static mp_uint s_main_volume;
static mp_uint s_sub_volume;
static mp_bool s_main_paused;
static mp_bool s_sub_paused;
static mp_bool s_main_loop;
static mp_uint s_sample_rate = 48000;
static mp_size s_alloc_size = 0;
static mp_s8 *s_alloc = NULL;


static xmp_context load_module(mp_uint module_id)
{
    xmp_context c = xmp_create_context();

    int s =  xmp_load_module_from_memory(c, mpt_module_banks[module_id],
                                         mpt_module_sizes[module_id]);
    if (s) // error!
    {
        xmp_free_context(c);
        return NULL;
    }

    if (xmp_start_player(c, s_sample_rate, XMP_FORMAT_8BIT) != 0)
    {
        xmp_free_context(c);
        return NULL;
    }

    xmp_set_player(c, XMP_PLAYER_INTERP, XMP_INTERP_NEAREST);
    xmp_set_player(c, XMP_PLAYER_MODE, XMP_MODE_FT2);

    return c;
}

static int process_module(xmp_context c, mp_uint volume, mp_bool loop,
                          mp_s8 *output, mp_uint frame_count)
{
    // i think maxmod outputs audio at a higher amplitude, because the kick
    // sounds more accurate to maxmod playback when 200 is the max volume factor
    // instead of 100.
    xmp_set_player(c, XMP_PLAYER_VOLUME, 200 * volume / VOLUME_SCALE);
    int stat = xmp_play_buffer(c, output, frame_count * sizeof(*output) * 2,
                               loop ? 0 : 1);

    return stat;
}

static inline void send_event(mp_msg_e msg, mp_uint param)
{
    if (!s_ev_handler) return;
    s_ev_handler(msg, param);
}


void mplay_init(void)
{
    s_main_volume = VOLUME_SCALE;
    s_sub_volume  = VOLUME_SCALE;

    s_main_paused = false;
    s_sub_paused = false;
}

void mplay_deinit(void)
{
    free(s_alloc);
}

void mplay_start(mp_uint module_id, mp_bool loop)
{
    s_main_xmpc = load_module(module_id);
    
    if (!s_main_xmpc)
    {
        fprintf(stderr, "mplay_start: could not start module!\n");
        return;
    }

    s_main_paused = false;
    s_main_loop = loop;
}

void mplay_set_sample_rate(mp_uint sample_rate)
{
    s_sample_rate = sample_rate;
}

void mplay_render(mp_s16 *data, mp_size frame_count)
{
    mp_size buffer_size = frame_count * sizeof(*data) * 2;
    memset(data, 0, buffer_size);

    if (s_alloc_size != buffer_size)
    {
        fprintf(stderr, "REALLOC MODPLAY BUFFER\n");

        free(s_alloc);
        s_alloc_size = buffer_size;
        s_alloc = malloc(buffer_size);
    }

    if (s_main_xmpc && !s_main_paused)
    {
        int stat = process_module(s_main_xmpc, s_main_volume, s_main_loop,
                                  s_alloc, frame_count);

        for (mp_size i = 0; i < frame_count * 2; ++i)
            data[i] += s_alloc[i];

        if (stat == -XMP_END)
        {
            send_event(MP_MSG_SONG_FINISHED, 0);
            xmp_end_player(s_main_xmpc);
            xmp_free_context(s_main_xmpc);
            s_main_xmpc = NULL;
        }
    }

    if (s_sub_xmpc && !s_sub_paused)
    {
        int stat = process_module(s_sub_xmpc, s_sub_volume, false,
                                  s_alloc, frame_count);

        for (mp_size i = 0; i < frame_count * 2; ++i)
            data[i] += s_alloc[i];

        if (stat == -XMP_END)
        {
            send_event(MP_MSG_SONG_FINISHED, 1);
            xmp_end_player(s_sub_xmpc);
            xmp_free_context(s_sub_xmpc);
            s_sub_xmpc = NULL;
        }
    }

    // post-process: "convert" to 9-bit audio
    // mp_s16 *p = data;
    // for (mp_size i = 0; i < frame_count; ++i, p += 2)
    // {
    //     p[0] = (p[0] - 64) / 128;
    //     p[1] = (p[1] - 64) / 128;

    //     p[0] = CLAMP(p[0], INT8_MIN, INT8_MAX+1) * 128;
    //     p[1] = CLAMP(p[1], INT8_MIN, INT8_MAX+1) * 128;
    // }
}

void mplay_pause(void)
{
    s_main_paused = true;
}

void mplay_resume(void)
{
    s_main_paused = false;
}

void mplay_stop(void)
{
    if (!s_main_xmpc) return;

    xmp_end_player(s_main_xmpc);
    xmp_free_context(s_main_xmpc);
    s_main_xmpc = NULL;
}

bool mplay_is_active(void)
{
    return s_main_xmpc != NULL;
}

void mplay_set_volume(mp_uint volume)
{
    // if (volume > VOLUME_SCALE) volume = VOLUME_SCALE;
    s_main_volume = volume;
}

void mplay_sub_start(mp_uint module_id)
{
    s_sub_xmpc = load_module(module_id);
    
    if (!s_sub_xmpc)
    {
        fprintf(stderr, "mplay_sub_start: could not start module!\n");
        return;
    }

    s_sub_paused = false;
    // s_sub_loop = false;
}

void mplay_set_sub_volume(mp_uint volume)
{
    s_sub_volume = volume;
}

void mplay_set_event_handler(mplay_event_handler_f handler)
{
    s_ev_handler = handler;
}