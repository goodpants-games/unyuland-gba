// platform (i.e. PC)-specific control

#ifndef PLATCTL_H
#define PLATCTL_H

#include <stdbool.h>

#define PLATCTL_VOLUME_MAX 1024 // (not *strictly* the max)

typedef void (*platctl_fulscr_change_watcher_f)(bool fulscr);

// "hardware" volume multiplier, in range [0, PLATCTL_VOLUME_MAX]
void platctl_set_volume(unsigned int volume);
void platctl_set_fullscreen(bool fulscr);
bool platctl_get_fullscreen(void);
void platctl_set_fullscreen_change_watcher(platctl_fulscr_change_watcher_f fun);
// int platctl_get_shader(void);
// void platctl_set_shader(int shader_index);

#endif