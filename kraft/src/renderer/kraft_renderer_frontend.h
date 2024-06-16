#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/kraft_renderer_imgui.h"
#include "containers/kraft_hashmap.h"
#include "resources/kraft_resource_types.h"

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
    HashMap<ResourceID, Array<Renderable>> Renderables;

    bool Init(ApplicationConfig* config);
    bool Shutdown();
    void OnResize(int width, int height);
    bool DrawFrame(RenderPacket* packet);
    bool AddRenderable(Renderable Object);

    // API
    Handle<Texture> GetSceneViewTexture();
    bool SetSceneViewViewportSize(uint32 Width, uint32 Height);
    void CreateRenderPipeline(Shader* Shader, int PassIndex);
    void DestroyRenderPipeline(Shader* Shader);
    void CreateMaterial(Material* Material);
    void DestroyMaterial(Material* Material);
    void UseShader(const Shader* Shader);
    void SetUniform(Shader* ActiveShader, const ShaderUniform& Uniform, void* Value, bool Invalidate);
    void ApplyGlobalShaderProperties(Shader* ActiveShader);
    void ApplyInstanceShaderProperties(Shader* ActiveShader);
    void DrawGeometry(uint32 GeometryID);
    bool CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize);
    void DestroyGeometry(Geometry* Geometry);
};

extern RendererFrontend* Renderer;

}

}