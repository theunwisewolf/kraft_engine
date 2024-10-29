#pragma once

#include "scenes/scene_types.h"
#include "world/kraft_world.h"

struct RendererImGui;

struct ApplicationState
{
    SceneState*    TestSceneState;
    kraft::World*  DefaultWorld;
    float64        LastTime = 0.0f;
    char           WindowTitleBuffer[1024];
    float64        TargetFrameTime = 1 / 60.f;
    uint64         FrameCount = 0;
    float64        TimeSinceLastFrame = 0.f;
    RendererImGui* ImGuiRenderer;
};

extern ApplicationState GlobalAppState;