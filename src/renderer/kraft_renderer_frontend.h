#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/kraft_renderer_imgui.h"

namespace kraft
{

struct ApplicationConfig;

struct RendererFrontend
{
    RendererBackend* Backend = nullptr;
    Block BackendMemory;
    RendererBackendType Type = RendererBackendType::RENDERER_BACKEND_TYPE_NONE;
    RendererImGui ImGuiRenderer;

    bool Init(ApplicationConfig* config);
    bool Shutdown();
    void OnResize(int width, int height);
    bool DrawFrame(RenderPacket* packet);

    // API
    void CreateTexture(uint8* data, Texture* out);
    void DestroyTexture(Texture* texture);
    void CreateMaterial(Material* material);
    void DestroyMaterial(Material* material);
};

extern RendererFrontend* Renderer;

}