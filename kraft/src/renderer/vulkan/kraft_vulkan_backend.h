#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Texture;
struct Shader;
struct ShaderUniform;
struct RendererOptions;
struct Geometry;
struct ArenaAllocator;

namespace r {

struct ShaderEffect;
struct VulkanContext;
struct CommandBuffer;
struct RenderPass;
struct RenderSurface;
struct Buffer;
struct GPUDevice;
struct GeometryDescription;

template<typename T>
struct Handle;

struct VulkanRendererBackend
{
    static bool Init(ArenaAllocator* arena, RendererOptions* options);
    static bool Shutdown();
    static int  PrepareFrame();
    static bool BeginFrame();
    static bool EndFrame();
    static void OnResize(i32 width, i32 height);

    static void CreateRenderPipeline(Shader* shader);
    static void DestroyRenderPipeline(Shader* shader);

    static void UseShader(const Shader* shader, u32 variant_index = 0);
    static void ApplyGlobalShaderProperties(Shader* shader, Handle<Buffer> ubo_buffer, Handle<Buffer> materials_buffer, Handle<Buffer> vertex_buffer, Handle<Buffer> index_buffer);
    static void ApplyLocalShaderProperties(Shader* shader, void* data);
    static void UpdateTextures(Handle<Texture>* textures, u64 texture_count);

    // Geometry
    static void DrawGeometryData(GeometryDrawData draw_data);
    static bool CreateGeometry(const GeometryDescription& description);
    static bool UpdateGeometry(const GeometryDescription& description);

    // Render Passes
    static void BeginSurface(RenderSurface* surface);
    static void EndSurface(RenderSurface* surface);

    // Commands
    static void CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx);

    // Misc
    static VulkanContext* Context();
    // static void           ImageBarrier(Handle<Texture> texture, VkDependencyFlags dependency_flags, VulkanImageBarrierDescription description);

    static void SetDeviceData(GPUDevice* device);
};

} // namespace r

} // namespace kraft