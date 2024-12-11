#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Texture;
struct Shader;
struct ShaderUniform;
struct Material;
struct EngineConfigT;
struct Geometry;

namespace renderer {

struct ShaderEffect;
struct VulkanContext;
struct CommandBuffer;
struct RenderPass;

template<typename T>
struct Handle;

namespace VulkanRendererBackend {

bool Init(EngineConfigT* config);
bool Shutdown();
int  PrepareFrame();
bool BeginFrame();
bool EndFrame();
void OnResize(int width, int height);

void CreateRenderPipeline(Shader* Shader, int PassIndex, Handle<RenderPass> RenderPassHandle);
void DestroyRenderPipeline(Shader* Shader);
void CreateMaterial(Material* Material);
void DestroyMaterial(Material* Material);

void UseShader(const Shader* Shader);
void SetUniform(Shader* Instance, const ShaderUniform& Uniform, void* Value, bool Invalidate);
void ApplyGlobalShaderProperties(Shader* Material);
void ApplyInstanceShaderProperties(Shader* Material);

// Geometry
void DrawGeometryData(uint32 GeometryID);
bool CreateGeometry(
    Geometry*    Geometry,
    uint32       VertexCount,
    const void*  Vertices,
    uint32       VertexSize,
    uint32       IndexCount,
    const void*  Indices,
    const uint32 IndexSize
);
void DestroyGeometry(Geometry* Geometry);

// Render Passes
void BeginRenderPass(Handle<CommandBuffer> CmdBuffer, Handle<RenderPass> PassHandle);
void EndRenderPass(Handle<CommandBuffer> CmdBuffer, Handle<RenderPass> PassHandle);

VulkanContext* Context();
};

}

}