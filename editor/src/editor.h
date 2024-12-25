#pragma once

#include <containers/kraft_array.h>
#include <core/kraft_core.h>
#include <world/kraft_entity_types.h>
#include <world/kraft_world.h>

#include <renderer/kraft_renderer_types.h>

#include "imgui/imgui_renderer.h"

namespace kraft {
struct Entity;
struct Camera;
}

struct ApplicationState
{
    float64       LastTime = 0.0f;
    char          WindowTitleBuffer[1024];
    float64       TargetFrameTime = 1 / 144.f;
    uint64        FrameCount = 0;
    float64       TimeSinceLastFrame = 0.f;
    RendererImGui ImGuiRenderer;
};

struct EditorCamera : kraft::Camera
{
    int     Flying = 0;
    float32 Sensitivity = 5.0f;
};

struct EditorState
{
    static EditorState*             Ptr;
    kraft::EntityHandleT            SelectedEntity = kraft::EntityHandleInvalid;
    kraft::World*                   CurrentWorld;
    kraft::renderer::RenderSurfaceT RenderSurface;
    EditorCamera                    ViewportCamera;

    EditorState();
    kraft::Entity GetSelectedEntity();

    void SetCamera(const EditorCamera& Camera);
};

extern ApplicationState GlobalAppState;