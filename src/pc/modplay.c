#include <stdio.h>
#include <stdlib.h>

#include <modplay.h>
#include <tonc.h>
#include <libopenmpt/libopenmpt.h>

#include <data/music.h>

extern const void *const mpt_module_banks[MODDAT_NSONGS];
extern const size_t mpt_module_sizes[MODDAT_NSONGS];

#define VOLUME_SCALE 1024

static mplay_event_handler_f s_ev_handler = NULL;
static openmpt_module *s_main_module = NULL;
static openmpt_module *s_sub_module = NULL;
static mp_uint s_main_volume;
static mp_uint s_sub_volume;
static mp_bool s_main_paused;
static mp_bool s_sub_paused;;
static mp_uint s_sample_rate = 48000;

static mp_size s_alloc_size = 0;
static mp_s16 *s_alloc = NULL;


static openmpt_module *load_module(mp_uint module_id, mp_bool loop)
{
    openmpt_module *mod;
    mod = openmpt_module_create_from_memory2(
        mpt_module_banks[module_id], mpt_module_sizes[module_id],
        NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    
    if (!mod) return NULL;

    openmpt_module_set_render_param(
        mod, OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH, 1);
    openmpt_module_ctl_set_integer(mod, "dither", 0);

    openmpt_module_set_repeat_count(mod, loop ? -1 : 0);

    return mod;
}

static mp_size process_module(openmpt_module *module, mp_uint volume,
                              s16 *output, mp_uint frame_count)
{
    mp_size rframes = openmpt_module_read_interleaved_stereo(
        module, (int32_t)s_sample_rate, frame_count, output);
    
    mp_s16 *p = output;
    for (mp_size i = 0; i < rframes; ++i)
    {
        // apply volume scale
        p[0] = (s16)(((s32)p[0]) * volume / VOLUME_SCALE);
        p[1] = (s16)(((s32)p[1]) * volume / VOLUME_SCALE);
        p += 2;
    }

    // fill the remaining samples with zeroes
    memset(output, 0, (frame_count - rframes) * sizeof(s16) * 2);

    return rframes;
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
    s_main_module = load_module(module_id, loop);
    
    if (!s_main_module)
    {
        fprintf(stderr, "mplay_start: could not load module!");
        return;
    }

    s_main_paused = false;
}

void mplay_set_sample_rate(mp_uint sample_rate)
{
    s_sample_rate = sample_rate;
}

void mplay_render(mp_s16 *data, mp_size frame_count)
{
    mp_size buffer_size = frame_count * sizeof(mp_s16) * 2;
    memset(data, 0, buffer_size);

    if (s_alloc_size != buffer_size)
    {
        fprintf(stderr, "REALLOC MODPLAY BUFFER");

        free(s_alloc);
        s_alloc_size = buffer_size;
        s_alloc = malloc(buffer_size);
    }

    if (s_main_module && !s_main_paused)
    {
        mp_size rframes = process_module(s_main_module, s_main_volume,
                                         s_alloc, frame_count);

        for (mp_size i = 0; i < frame_count * 2; ++i)
            data[i] += s_alloc[i];

        if (rframes == 0)
        {
            send_event(MP_MSG_SONG_FINISHED, 0);
            openmpt_module_destroy(s_main_module);
            s_main_module = NULL;
        }
    }

    if (s_sub_module && !s_sub_paused)
    {
        mp_size rframes = process_module(s_sub_module, s_sub_volume,
                                         s_alloc, frame_count);

        for (mp_size i = 0; i < frame_count * 2; ++i)
            data[i] += s_alloc[i];

        if (rframes == 0)
        {
            send_event(MP_MSG_SONG_FINISHED, 1);
            openmpt_module_destroy(s_sub_module);
            s_sub_module = NULL;
        }
    }

    // post-process: "convert" to 9-bit audio
    mp_s16 *p = data;
    for (mp_size i = 0; i < frame_count; ++i, p += 2)
    {
        p[0] = (p[0] - 64) / 128;
        p[1] = (p[1] - 64) / 128;

        p[0] = CLAMP(p[0], INT8_MIN, INT8_MAX+1) * 128;
        p[1] = CLAMP(p[1], INT8_MIN, INT8_MAX+1) * 128;
    }
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
    if (!s_main_module) return;

    openmpt_module_destroy(s_main_module);
    s_main_module = NULL;
}

bool mplay_is_active(void)
{
    return s_main_module != NULL;
}

void mplay_set_volume(mp_uint volume)
{
    // if (volume > VOLUME_SCALE) volume = VOLUME_SCALE;
    s_main_volume = volume;
}

void mplay_sub_start(mp_uint module_id)
{
    s_sub_module = load_module(module_id, false);
    
    if (!s_sub_module)
    {
        fprintf(stderr, "mplay_sub_start: could not load module!");
        return;
    }

    s_sub_paused = false;
}

void mplay_set_sub_volume(mp_uint volume)
{
    s_sub_volume = volume;
}

void mplay_set_event_handler(mplay_event_handler_f handler)
{
    s_ev_handler = handler;
}