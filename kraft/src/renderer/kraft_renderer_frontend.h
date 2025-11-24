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

namespace r {

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
    void DrawSingle(Shader* shader, GlobalShaderData* GlobalUBO, u32 GeometryId);
    void Draw(GlobalShaderData* global_ubo);
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
    void DrawGeometry(u32 GeometryID);
    bool CreateGeometry(Geometry* Geometry, u32 VertexCount, const void* Vertices, u32 VertexSize, u32 IndexCount, const void* Indices, const u32 IndexSize);
    void DestroyGeometry(Geometry* Geometry);

    RenderSurfaceT CreateRenderSurface(const char* Name, u32 Width, u32 Height, bool HasDepth = false);
    void           BeginRenderSurface(const RenderSurfaceT& Surface);
    RenderSurfaceT ResizeRenderSurface(RenderSurfaceT& Surface, u32 Width, u32 Height);
    void           EndRenderSurface(const RenderSurfaceT& Surface);

    void CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx);
};

RendererFrontend* CreateRendererFrontend(const RendererOptions* Opts);
void              DestroyRendererFrontend(RendererFrontend* Instance);

} // namespace r

extern r::RendererFrontend* g_Renderer;
extern r::GPUDevice*        g_Device;

} // namespace kraft