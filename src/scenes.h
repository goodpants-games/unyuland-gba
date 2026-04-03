#ifndef SCENES_H
#define SCENES_H

#include <stdint.h>

typedef struct scene_desc
{
    void (*load)(uintptr_t data);
    void (*unload)(void);
    void (*frame)(void);
}
scene_desc_s;

extern const scene_desc_s scene_desc_menu;
extern const scene_desc_s scene_desc_game;
extern const scene_desc_s scene_desc_intro;
extern const scene_desc_s scene_desc_end;

extern const scene_desc_s *scenemgr_current;

void scenemgr_init(const scene_desc_s *init_scene, uintptr_t data);
void scenemgr_change(const scene_desc_s *scene, uintptr_t data);
void scenemgr_frame(void);

#endif