#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/kraft_renderer_imgui.h"

namespace kraft
{

struct ApplicationConfig;

namespace renderer
{

struct ShaderEffect;

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
    void CreateRenderPipeline(Shader* Shader, int PassIndex);
    void DestroyRenderPipeline(Shader* Shader);
    void CreateTexture(uint8* data, Texture* out);
    void DestroyTexture(Texture* texture);
    void CreateMaterial(Material* material);
    void DestroyMaterial(Material* material);
    void UseShader(const Shader* Shader);
    void SetUniform(Shader* Shader, const ShaderUniform& Uniform, void* Value, bool Invalidate);
    void ApplyGlobalShaderProperties(Shader* Shader);
    void ApplyInstanceShaderProperties(Shader* Shader);
    void DrawGeometry(GeometryRenderData Data);
    bool CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize);
    void DestroyGeometry(Geometry* Geometry);
};

extern RendererFrontend* Renderer;

}

}