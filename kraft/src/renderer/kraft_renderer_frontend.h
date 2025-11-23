#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct World;
struct EngineConfig;
struct Camera;
struct Shader;
struct Geometry;
struct Material;
struct ShaderUniform;
struct Texture;
struct RendererOptions;

namespace renderer {

struct RenderSurfaceT;
struct ShaderEffect;
struct RendererBackend;
struct Renderable;
struct RenderPass;
struct Buffer;
struct MeshMaterial;
struct GlobalShaderData;
struct GPUDevice;

template<typename T>
struct Handle;

struct RendererFrontend
{
    struct RendererOptions* Settings;
    kraft::Camera*          Camera = nullptr;

    void Init();
    void Draw(Shader* Shader, GlobalShaderData* GlobalUBO, uint32 GeometryId);
    void OnResize(int Width, int Height);
    void PrepareFrame();
    bool DrawSurfaces();
    bool AddRenderable(const Renderable& Object);

    void BeginMainRenderpass();
    void EndMainRenderpass();

    // API
    void CreateRenderPipeline(Shader* Shader, Handle<RenderPass> RenderPassHandle);
    void DestroyRenderPipeline(Shader* Shader);
    void UseShader(const Shader* Shader);
    void ApplyGlobalShaderProperties(Shader* ActiveShader, Handle<Buffer> GlobalUBOBuffer, Handle<Buffer> GlobalMaterialsBuffer);
    void ApplyLocalShaderProperties(Shader* ActiveShader, void* Data);
    void DrawGeometry(uint32 GeometryID);
    bool CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize);
    void DestroyGeometry(Geometry* Geometry);

    RenderSurfaceT CreateRenderSurface(const char* Name, uint32 Width, uint32 Height, bool HasDepth = false);
    void           BeginRenderSurface(const RenderSurfaceT& Surface);
    RenderSurfaceT ResizeRenderSurface(RenderSurfaceT& Surface, uint32 Width, uint32 Height);
    void           EndRenderSurface(const RenderSurfaceT& Surface);

    void CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, uint32 set_idx, uint32 binding_idx);
};

RendererFrontend* CreateRendererFrontend(const RendererOptions* Opts);
void              DestroyRendererFrontend(RendererFrontend* Instance);

} // namespace renderer

extern renderer::RendererFrontend* g_Renderer;
extern renderer::GPUDevice*        g_Device;

} // namespace kraft