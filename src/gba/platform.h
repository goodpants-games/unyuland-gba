#ifndef PLATWIN_H
#define PLATWIN_H

#include <stdbool.h>
#include <tonc_bios.h>

#define PLATFORM_GBA

static inline bool platform_init() { return true; }
static inline void platform_close() { }
static inline bool platform_is_running() { return true; }
static inline void platform_submit_frame() { VBlankIntrWait(); }

#endif