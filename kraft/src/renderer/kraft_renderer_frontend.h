#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct World;
struct EngineConfigT;
struct Camera;
struct Shader;
struct Geometry;
struct Material;
struct ShaderUniform;
struct Texture;

namespace renderer {

struct RenderSurfaceT;
struct ShaderEffect;
struct RendererBackend;
struct Renderable;
struct RenderPass;
template<typename T>
struct Handle;

struct RendererFrontend
{
    kraft::Camera* Camera = nullptr;

    bool Init(EngineConfigT* Config);
    bool Shutdown();
    void OnResize(int Width, int Height);
    bool DrawFrame();
    bool AddRenderable(const Renderable& Object);

    bool BeginMainRenderpass();
    bool EndMainRenderpass();

    // API
    void            CreateRenderPipeline(Shader* Shader, int PassIndex, Handle<RenderPass> RenderPassHandle);
    void            DestroyRenderPipeline(Shader* Shader);
    void            CreateMaterial(Material* Material);
    void            DestroyMaterial(Material* Material);
    void            UseShader(const Shader* Shader);
    void            SetUniform(Shader* ActiveShader, const ShaderUniform& Uniform, void* Value, bool Invalidate);
    void            ApplyGlobalShaderProperties(Shader* ActiveShader);
    void            ApplyInstanceShaderProperties(Shader* ActiveShader);
    void            DrawGeometry(uint32 GeometryID);
    bool            CreateGeometry(
                   Geometry*    Geometry,
                   uint32       VertexCount,
                   const void*  Vertices,
                   uint32       VertexSize,
                   uint32       IndexCount,
                   const void*  Indices,
                   const uint32 IndexSize
               );
    void DestroyGeometry(Geometry* Geometry);

    RenderSurfaceT CreateRenderSurface(const char* Name, uint32 Width, uint32 Height, bool HasDepth = false);
    void BeginRenderSurface(const RenderSurfaceT& Surface);
    RenderSurfaceT ResizeRenderSurface(RenderSurfaceT& Surface, uint32 Width, uint32 Height);
    void EndRenderSurface(const RenderSurfaceT& Surface);
};

extern RendererFrontend* Renderer;

}

}