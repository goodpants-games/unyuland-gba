#ifndef MODPLAY_H
#define MODPLAY_H

#include <stdbool.h>

typedef unsigned int uint;

void mplay_init(void);

#ifdef PLATFORM_GBA
__attribute__((section(".iwram"), long_call, target("arm")))
void mplay_vblank_handler(void);
#endif

void mplay_start(uint module_id, bool loop);
void mplay_pause(void);
void mplay_resume(void);
void mplay_stop(void);
bool mplay_is_active(void);
void mplay_set_volume(uint volume);

void mplay_sub_start(uint module_id);
void mplay_set_sub_volume(uint volume);

#endif