#ifndef PLATCTL_H
#define PLATCTL_H

#define PLATCTL_VOLUME_MAX 1024 // (not *strictly* the max)

// platform (i.e. PC)-specific control

// "hardware" volume multiplier, in range [0, PLATCTL_VOLUME_MAX]
void platctl_set_volume(unsigned int volume);
// void platctl_set_fullscreen(bool fulscr);
// bool platctl_get_fullscreen(void);
// int platctl_get_shader(void);
// void platctl_set_shader(int shader_index);

#endif