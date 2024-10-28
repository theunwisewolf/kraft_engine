#include "scene_types.h"

#include "core/kraft_memory.h"
#include "world/kraft_world.h"

SceneState* TestSceneState = nullptr;
kraft::World* DefaultWorld = nullptr;

void SetupScene()
{
    TestSceneState = (SceneState*)kraft::Malloc(sizeof(SceneState), kraft::MEMORY_TAG_NONE, true);
    TestSceneState->SceneCamera = kraft::Camera();

    DefaultWorld = (kraft::World*)kraft::Malloc(sizeof(kraft::World), kraft::MEMORY_TAG_NONE, true);
    new (DefaultWorld) kraft::World();
}

void DestroyScene()
{
    kraft::Free(TestSceneState, sizeof(SceneState), kraft::MEMORY_TAG_NONE);
    kraft::Free(DefaultWorld, sizeof(kraft::World), kraft::MEMORY_TAG_NONE);
}