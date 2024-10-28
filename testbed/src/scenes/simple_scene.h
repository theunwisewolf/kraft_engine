#pragma once

#include "scene_types.h"
#include "world/kraft_world.h"

extern SceneState* TestSceneState;
extern kraft::World* DefaultWorld;

void SetupScene();
void DestroyScene();