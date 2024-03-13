#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/kraft_renderer_imgui.h"
#include "renderer/shaderfx/kraft_shaderfx.h"

namespace kraft
{

struct ApplicationConfig;

namespace renderer
{

struct RendererFrontend
{
    kraft::Block BackendMemory;
    RendererBackend* Backend = nullptr;
    RendererBackendType Type = RendererBackendType::RENDERER_BACKEND_TYPE_NONE;
    RendererImGui ImGuiRenderer;

    bool Init(ApplicationConfig* config);
    bool Shutdown();
    void OnResize(int width, int height);
    bool DrawFrame(RenderPacket* packet);

    // API
    RenderPipeline CreateRenderPipeline(const ShaderEffect& Effect, int PassIndex);
    void CreateTexture(uint8* data, Texture* out);
    void DestroyTexture(Texture* texture);
    void CreateMaterial(Material* material);
    void DestroyMaterial(Material* material);
    void DrawGeometry(GeometryRenderData Data);
    bool CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 IndexCount, const void* Indices);
    void DestroyGeometry(Geometry* Geometry);
};

extern RendererFrontend* Renderer;

}

}