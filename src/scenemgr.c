#include "scenes.h"
#include <stddef.h>
#include <stdbool.h>

const scene_desc_s *scenemgr_current = NULL;

struct
{
    const scene_desc_s *scene;
    uintptr_t data;
} static scene_change_data;

static void commit_scene_change(void)
{
    if (scenemgr_current && scenemgr_current->unload)
        scenemgr_current->unload();

    scenemgr_current = scene_change_data.scene;
    if (scenemgr_current && scenemgr_current->load)
        scenemgr_current->load(scene_change_data.data);
    
    scene_change_data.scene = NULL;
}

void scenemgr_init(const scene_desc_s *init_scene, uintptr_t data)
{
    scene_change_data.scene = NULL;

    scenemgr_current = init_scene;
    if (scenemgr_current && scenemgr_current->load)
        scenemgr_current->load(data);
}

void scenemgr_change(const scene_desc_s *scene, uintptr_t data)
{
    scene_change_data.scene = scene;
    scene_change_data.data = data;
}

void scenemgr_frame(void)
{
    if (scene_change_data.scene)
        commit_scene_change();

    if (scenemgr_current && scenemgr_current->frame)
        scenemgr_current->frame();
}