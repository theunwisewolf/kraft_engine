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

struct RenderSurface;
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
    void CreateRenderPipeline(Shader* shader);
    void DestroyRenderPipeline(Shader* shader);
    void UseShader(const Shader* shader, u32 variant_index = 0);
    void ApplyGlobalShaderProperties(Shader* ActiveShader, Handle<Buffer> GlobalUBOBuffer, Handle<Buffer> GlobalMaterialsBuffer);
    void ApplyLocalShaderProperties(Shader* ActiveShader, void* Data);
    void DrawGeometry(u32 id);
    bool CreateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size);
    bool UpdateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size);
    void DestroyGeometry(Geometry* geometry);

    RenderSurface CreateRenderSurface(String8 name, u32 width, u32 height, bool has_color = true, bool has_depth = false, bool depth_sample = false);
    void          BeginRenderSurface(const RenderSurface& surface);
    RenderSurface ResizeRenderSurface(RenderSurface& surface, u32 width, u32 height);
    void          EndRenderSurface(const RenderSurface& surface);

    void CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx);
};

RendererFrontend* CreateRendererFrontend(const RendererOptions* options);
void              DestroyRendererFrontend(RendererFrontend* instance);

} // namespace r

extern r::RendererFrontend* g_Renderer;
extern r::GPUDevice*        g_Device;

} // namespace kraft