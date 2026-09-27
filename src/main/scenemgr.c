/*
Copyright (C) 2026  pkhead

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

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