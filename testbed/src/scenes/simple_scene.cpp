#include "scene_types.h"

#include "core/kraft_memory.h"

SceneState* TestSceneState = nullptr;

void SetupScene()
{
    TestSceneState = (SceneState*)kraft::Malloc(sizeof(SceneState), kraft::MEMORY_TAG_NONE, true);
    TestSceneState->SceneCamera = kraft::Camera();
}

void DestroyScene()
{
    kraft::Free(TestSceneState, sizeof(SceneState), kraft::MEMORY_TAG_NONE);
}