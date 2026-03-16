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
struct GeometryDrawData;
struct RendererBackend;
struct Renderable;
struct RenderPass;
struct Buffer;
struct MeshMaterial;
struct GlobalShaderData;
struct GPUDevice;

template<typename T>
struct Handle;

struct SpriteBatch
{
    Vertex2D* vertices;
    u32*      indices;
    Geometry* geometry;
    Material* material;
    u16       quad_count;
    u16       max_quad_count;
};

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
    bool AddRenderable(const Renderable& object);
    bool AddRenderable(SpriteBatch* batch);

    void BeginMainRenderpass();
    void EndMainRenderpass();

    // API
    void CreateRenderPipeline(Shader* shader);
    void DestroyRenderPipeline(Shader* shader);
    void UseShader(const Shader* shader, u32 variant_index = 0);
    void ApplyGlobalShaderProperties(
        Shader*        shader,
        Handle<Buffer> ubo_buffer,
        Handle<Buffer> materials_buffer,
        Handle<Buffer> vertex_buffer,
        Handle<Buffer> index_buffer
    );
    void ApplyLocalShaderProperties(Shader* shader, void* data);
    void DrawGeometry(const GeometryDrawData& draw_data);
    bool CreateGeometry(
        Geometry*   geometry,
        u32         vertex_count,
        const void* vertices,
        u32         vertex_size,
        u32         index_count,
        const void* indices,
        const u32   index_size
    );
    bool UpdateGeometry(
        Geometry*   geometry,
        u32         vertex_count,
        const void* vertices,
        u32         vertex_size,
        u32         index_count,
        const void* indices,
        const u32   index_size
    );

    RenderSurface CreateRenderSurface(
        String8 name,
        u32     width,
        u32     height,
        bool    has_color = true,
        bool    has_depth = false,
        bool    depth_sample = false
    );
    void          BeginRenderSurface(const RenderSurface& surface);
    RenderSurface ResizeRenderSurface(RenderSurface& surface, u32 width, u32 height);
    void          EndRenderSurface(const RenderSurface& surface);

    void CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx);
};

RendererFrontend* CreateRendererFrontend(const RendererOptions* options);
void              DestroyRendererFrontend(RendererFrontend* instance);

SpriteBatch* CreateSpriteBatch(ArenaAllocator* arena, u16 batch_size);
void         BeginSpriteBatch(SpriteBatch* batch, Material* material);
bool         DrawQuad(SpriteBatch* batch, vec2 position, vec2 size, vec2 uv_min, vec2 uv_max, vec4 color);
bool         DrawQuad(SpriteBatch* batch, vec2 p0, vec2 p1, vec2 p2, vec2 p3, vec2 uv_min, vec2 uv_max, vec4 color);
void         EndSpriteBatch(SpriteBatch* batch);

} // namespace r

extern r::RendererFrontend* g_Renderer;
extern r::GPUDevice*        g_Device;

} // namespace kraft