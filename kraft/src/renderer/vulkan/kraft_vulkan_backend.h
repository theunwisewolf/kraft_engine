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
struct GlobalShaderData;
struct Buffer;
struct GPUDevice;

template<typename T>
struct Handle;

struct VulkanRendererBackend
{
    static bool Init(ArenaAllocator* arena, RendererOptions* options);
    static bool Shutdown();
    static int  PrepareFrame();
    static bool BeginFrame();
    static bool EndFrame();
    static void OnResize(int width, int height);

    static void CreateRenderPipeline(Shader* shader, Handle<RenderPass> render_pass);
    static void DestroyRenderPipeline(Shader* shader);

    static void SetGlobalShaderData(GlobalShaderData* Data);
    static void UseShader(const Shader* Shader);
    static void ApplyGlobalShaderProperties(Shader* shader, Handle<Buffer> global_ubo, Handle<Buffer> global_materials_buffer);
    static void ApplyLocalShaderProperties(Shader* shader, void* data);
    static void UpdateTextures(Handle<Texture>* textures, u64 texture_count);

    // Geometry
    static void DrawGeometryData(u32 id);
    static bool CreateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size);
    static bool UpdateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size);
    static void DestroyGeometry(Geometry* geometry);

    // Render Passes
    static void BeginRenderPass(Handle<CommandBuffer> cmd_buffer, Handle<RenderPass> pass);
    static void EndRenderPass(Handle<CommandBuffer> cmd_buffer, Handle<RenderPass> pass);

    // Commands
    static void CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx);

    // Misc
    static VulkanContext* Context();

    static void SetDeviceData(GPUDevice* device);
};

} // namespace r

} // namespace kraft