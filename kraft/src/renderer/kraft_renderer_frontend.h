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

namespace renderer {

struct ShaderEffect;
struct RendererBackend;
struct Renderable;
template<typename T>
struct Handle;

struct RendererFrontend
{
    kraft::Camera* Camera = nullptr;

    bool Init(EngineConfig* Config);
    bool Shutdown();
    void OnResize(int Width, int Height);
    bool DrawFrame();
    bool AddRenderable(const Renderable& Object);

    bool BeginMainRenderpass();
    bool EndMainRenderpass();

    // API
    Handle<Texture> GetSceneViewTexture();
    bool            SetSceneViewViewportSize(uint32 Width, uint32 Height);
    void            CreateRenderPipeline(Shader* Shader, int PassIndex);
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
};

extern RendererFrontend* Renderer;

}

}