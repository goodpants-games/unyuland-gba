#ifndef MODPLAY_H
#define MODPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int          mp_int;
typedef unsigned int mp_uint;
typedef uint32_t     mp_u32;
typedef int32_t      mp_s32;
typedef uint16_t     mp_u16;
typedef int16_t      mp_s16;
typedef bool         mp_bool;
typedef size_t       mp_size;

typedef enum
{
    MP_MSG_SONG_FINISHED    
} mp_msg_e;

typedef void (*mplay_event_handler_f)(mp_msg_e msg, mp_int param);

void mplay_init(void);

#ifdef PLATFORM_GBA
__attribute__((section(".iwram"), long_call, target("arm")))
void mplay_vblank_handler(void);
#endif

#ifdef PLATFORM_PC
void mplay_set_sample_rate(mp_uint sample_rate);
void mplay_render(mp_s16 *data, mp_size frame_count);
void mplay_deinit(void);
#endif

void mplay_start(mp_uint module_id, mp_bool loop);
void mplay_pause(void);
void mplay_resume(void);
void mplay_stop(void);
bool mplay_is_active(void);
void mplay_set_volume(mp_uint volume);

void mplay_sub_start(mp_uint module_id);
void mplay_set_sub_volume(mp_uint volume);

void mplay_set_event_handler(mplay_event_handler_f handler);

#endif