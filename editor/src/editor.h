#pragma once

#include "core/kraft_core.h"
#include "world/kraft_entity_types.h"
#include "world/kraft_world.h"

namespace kraft {
struct Entity;
}

struct RendererImGui;

struct ApplicationState
{
    float64        LastTime = 0.0f;
    char           WindowTitleBuffer[1024];
    float64        TargetFrameTime = 1 / 60.f;
    uint64         FrameCount = 0;
    float64        TimeSinceLastFrame = 0.f;
    RendererImGui* ImGuiRenderer;
};

struct EditorState
{
    static EditorState*  Ptr;
    kraft::EntityHandleT SelectedEntity = kraft::EntityHandleInvalid;
    kraft::World*        CurrentWorld;

    EditorState()
    {
        EditorState::Ptr = this;
    }

    kraft::Entity& GetSelectedEntity();
};

extern ApplicationState GlobalAppState;