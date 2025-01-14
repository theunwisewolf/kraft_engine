#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Texture;
struct Shader;
struct ShaderUniform;
struct Material;
struct RendererOptions;
struct Geometry;
struct ArenaAllocator;

namespace renderer {

struct ShaderEffect;
struct VulkanContext;
struct CommandBuffer;
struct RenderPass;
struct GlobalShaderData;
struct Buffer;

template<typename T>
struct Handle;

struct VulkanRendererBackend
{
    static bool Init(ArenaAllocator* Arena, RendererOptions* Opts);
    static bool Shutdown();
    static int  PrepareFrame();
    static bool BeginFrame();
    static bool EndFrame();
    static void OnResize(int width, int height);

    static void CreateRenderPipeline(Shader* Shader, int PassIndex, Handle<RenderPass> RenderPassHandle);
    static void DestroyRenderPipeline(Shader* Shader);
    static void CreateMaterial(Material* Material);
    static void DestroyMaterial(Material* Material);

    static void SetGlobalShaderData(GlobalShaderData* Data);
    static void UseShader(const Shader* Shader);
    static void SetUniform(Shader* Instance, const ShaderUniform& Uniform, void* Value, bool Invalidate);
    static void ApplyGlobalShaderProperties(Shader* Shader, Handle<Buffer> GlobalUBOBuffer, Handle<Buffer> GlobalMaterialsBuffer);
    static void ApplyInstanceShaderProperties(Shader* Shader);
    static void ApplyLocalShaderProperties(Shader* Shader, void* Data);

    // Geometry
    static void DrawGeometryData(uint32 GeometryID);
    static bool CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize);
    static void DestroyGeometry(Geometry* Geometry);

    // Render Passes
    static void BeginRenderPass(Handle<CommandBuffer> CmdBuffer, Handle<RenderPass> PassHandle);
    static void EndRenderPass(Handle<CommandBuffer> CmdBuffer, Handle<RenderPass> PassHandle);

    // Misc
    static bool           ReadObjectPickingBuffer(uint32** OutBuffer, uint32* BufferSize);
    static VulkanContext* Context();
};

}

}