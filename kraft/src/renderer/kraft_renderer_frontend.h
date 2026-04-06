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
struct ArenaAllocator;

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

template <typename T> struct Handle;

struct SpriteBatch {
    Vertex2D* vertices;
    u32* indices;
    Geometry* geometry;
    Material* material;
    u16 quad_count;
    u16 max_quad_count;
};

struct RendererUploadStats {
    f64 total_ms = 0.0;
    u32 call_count = 0;
    u32 block_count = 0;
    u32 touched_block_count = 0;
    u64 bytes_uploaded = 0;
    u64 bytes_used = 0;
};

struct RendererDynamicGeometryStats {
    f64 allocation_total_ms = 0.0;
    f64 upload_total_ms = 0.0;
    u32 allocation_count = 0;
    u64 vertex_bytes_requested = 0;
    u64 index_bytes_requested = 0;
    RendererUploadStats vertex_upload;
    RendererUploadStats index_upload;
};

struct RendererPrepareFrameStats {
    f64 total_ms = 0.0;
    f64 resource_end_frame_ms = 0.0;
    f64 backend_prepare_frame_ms = 0.0;
    f64 material_stage_memcpy_ms = 0.0;
    f64 material_upload_ms = 0.0;
    f64 texture_update_ms = 0.0;
    f64 allocator_reset_ms = 0.0;
    u32 dirty_texture_count = 0;
    bool materials_uploaded = false;
};

struct RendererSurfaceStats {
    f64 begin_total_ms = 0.0;
    f64 submit_total_ms = 0.0;
    f64 end_total_ms = 0.0;
    f64 begin_surface_ms = 0.0;
    f64 end_surface_ms = 0.0;
    f64 global_ubo_copy_ms = 0.0;
    f64 set_active_variant_ms = 0.0;
    f64 sort_ms = 0.0;
    f64 initial_shader_bind_ms = 0.0;
    f64 global_properties_ms = 0.0;
    f64 pipeline_switch_ms = 0.0;
    f64 custom_bind_group_ms = 0.0;
    f64 local_properties_ms = 0.0;
    f64 draw_geometry_ms = 0.0;
    f64 reset_active_variant_ms = 0.0;
    u32 begin_count = 0;
    u32 submit_count = 0;
    u32 end_count = 0;
    u32 draw_call_count = 0;
    u32 pipeline_bind_count = 0;
    u32 global_properties_count = 0;
    u32 custom_bind_group_bind_count = 0;
    u32 local_properties_count = 0;
};

struct RendererSlugTextStats {
    f64 total_ms = 0.0;
    f64 build_geometry_ms = 0.0;
    f64 glyph_resolve_ms = 0.0;
    f64 kerning_ms = 0.0;
    f64 quad_build_ms = 0.0;
    f64 allocate_dynamic_geometry_ms = 0.0;
    f64 vertex_memcpy_ms = 0.0;
    f64 index_memcpy_ms = 0.0;
    f64 material_update_ms = 0.0;
    u32 call_count = 0;
    u32 quad_count = 0;
    u64 text_byte_count = 0;
    u64 vertex_bytes_generated = 0;
    u64 index_bytes_generated = 0;
};

struct RendererFrameStats {
    u64 frame_id = 0;
    f64 total_ms = 0.0;
    f64 prepare_frame_ms = 0.0;
    f64 begin_main_renderpass_ms = 0.0;
    f64 end_main_renderpass_ms = 0.0;
    RendererPrepareFrameStats prepare_frame;
    RendererDynamicGeometryStats dynamic_geometry;
    RendererSurfaceStats surfaces;
    RendererSlugTextStats slug_text;
};

struct RendererStats {
    RendererFrameStats current_frame;
    RendererFrameStats last_completed_frame;
};

struct RendererFrontend {
    struct RendererOptions* Settings;
    kraft::Camera* Camera = nullptr;
    RendererStats Stats;
    u64 StatsFrameCounter = 0;

    void Init();
    void DrawSingle(Shader* shader, GlobalShaderData* global_ubo, u32 geometry_id);
    void OnResize(int width, int height);
    void PrepareFrame();

    RenderSurface* GetMainSurface();
    void BeginMainRenderpass();
    void EndMainRenderpass();

    // API
    void CreateRenderPipeline(Shader* shader);
    void DestroyRenderPipeline(Shader* shader);
    void UseShader(const Shader* shader, u32 variant_index = 0);
    void ApplyGlobalShaderProperties(Shader* shader, Handle<Buffer> ubo_buffer, Handle<Buffer> materials_buffer);
    void ApplyLocalShaderProperties(Shader* shader, void* data);
    void DrawGeometry(const GeometryDrawData& draw_data, Handle<Buffer> index_buffer);
    bool CreateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size);
    bool UpdateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size);

    RenderSurface CreateRenderSurface(String8 name, u32 width, u32 height, bool has_color = true, bool has_depth = false, bool depth_sample = false);
    RenderSurface ResizeRenderSurface(RenderSurface& surface, u32 width, u32 height);

    // Bump-allocates vertex + index space from the per-frame host-visible dynamic buffers.
    // Call each frame for geometry that changes every frame (text, particles, UI).
    // vertex_stride: sizeof your vertex type (used for VertexOffset calculation and alignment).
    DynamicGeometryAllocation AllocateDynamicGeometry(u32 vertex_count, u32 vertex_stride, u32 index_count);

    void CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx);

    // Debug primitives
    Geometry* CreatePlaneGeometry(f32 uv_scale = 1.0f);

    const RendererStats& GetStats() const;
    void RecordSlugTextStats(const RendererSlugTextStats& stats);
};

RendererFrontend* CreateRendererFrontend(const RendererOptions* options);
void DestroyRendererFrontend(RendererFrontend* instance);

SpriteBatch* CreateSpriteBatch(ArenaAllocator* arena, u16 batch_size);
void BeginSpriteBatch(SpriteBatch* batch, Material* material);
bool DrawQuad(SpriteBatch* batch, vec2 position, vec2 size, vec2 uv_min, vec2 uv_max, vec4 color);
bool DrawQuad(SpriteBatch* batch, vec2 p0, vec2 p1, vec2 p2, vec2 p3, vec2 uv_min, vec2 uv_max, vec4 color);
void EndSpriteBatch(SpriteBatch* batch);

} // namespace r

extern r::RendererFrontend* g_Renderer;
extern r::GPUDevice* g_Device;

} // namespace kraft
