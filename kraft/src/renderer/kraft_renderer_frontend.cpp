#include "kraft_renderer_frontend.h"

#include <containers/kraft_hashmap.h>
#include <core/kraft_asserts.h>
#include <core/kraft_engine.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <renderer/kraft_camera.h>
#include <renderer/kraft_renderer_backend.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <renderer/kraft_resource_manager.h>

#include <resources/kraft_resource_types.h>
#include <renderer/shaderfx/kraft_shaderfx_types.h>

namespace kraft::renderer {

struct RendererDataT
{
    kraft::Block                           BackendMemory;
    RendererBackendType                    Type = RendererBackendType::RENDERER_BACKEND_TYPE_NONE;
    HashMap<ResourceID, Array<Renderable>> Renderables;
    RendererBackend*                       Backend = nullptr;
} RendererData;

RendererFrontend* Renderer = nullptr;

bool RendererFrontend::Init(EngineConfigT* Config)
{
    Renderer = this;
    RendererData.Type = Config->RendererBackend;
    RendererData.BackendMemory = MallocBlock(sizeof(RendererBackend), MEMORY_TAG_RENDERER);
    RendererData.Backend = (RendererBackend*)RendererData.BackendMemory.Data;
    RendererData.Renderables.reserve(1024);

    KASSERTM(RendererData.Type != RendererBackendType::RENDERER_BACKEND_TYPE_NONE, "No renderer backend specified");

    if (!CreateBackend(RendererData.Type, RendererData.Backend))
    {
        return false;
    }

    if (!RendererData.Backend->Init(Config))
    {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return false;
    }

    return true;
}

bool RendererFrontend::Shutdown()
{
    DestroyBackend(RendererData.Backend);

    FreeBlock(RendererData.BackendMemory);
    RendererData.Backend = nullptr;

    return true;
}

void RendererFrontend::OnResize(int width, int height)
{
    if (RendererData.Backend)
    {
        RendererData.Backend->OnResize(width, height);
    }
    else
    {
        KERROR("[RendererFrontend::OnResize]: No backend!");
    }
}

bool RendererFrontend::DrawFrame()
{
    static uint64 Frame = 0;

    Mat4f ProjectionMatrix = this->Camera->ProjectionMatrix;
    Mat4f ViewMatrix = this->Camera->GetViewMatrix();

    RendererData.Backend->PrepareFrame();
    RendererData.Backend->BeginSceneView();
    for (auto It = RendererData.Renderables.vbegin(); It != RendererData.Renderables.vend(); It++)
    {
        auto&  Objects = *It;
        uint64 Count = Objects.Size();
        if (!Count)
            continue;
        ShaderSystem::Bind(Objects[0].MaterialInstance->Shader);
        ShaderSystem::ApplyGlobalProperties(ProjectionMatrix, ViewMatrix);
        for (int i = 0; i < Count; i++)
        {
            Renderable Object = Objects[i];
            ShaderSystem::SetMaterialInstance(Object.MaterialInstance);

            MaterialSystem::ApplyInstanceProperties(Object.MaterialInstance);
            MaterialSystem::ApplyLocalProperties(Object.MaterialInstance, Object.ModelMatrix);

            RendererData.Backend->DrawGeometryData(Object.GeometryID);
        }
        ShaderSystem::Unbind();

        Objects.Clear();
    }
    RendererData.Backend->EndSceneView();

    return true;
}

bool RendererFrontend::AddRenderable(const Renderable& Object)
{
    ResourceID Key = Object.MaterialInstance->Shader->ID;
    if (!RendererData.Renderables.has(Key))
    {
        RendererData.Renderables[Key] = Array<Renderable>();
        RendererData.Renderables[Key].Reserve(1024);
    }

    RendererData.Renderables[Key].Push(Object);

    return true;
}

bool RendererFrontend::BeginMainRenderpass()
{
    return RendererData.Backend->BeginFrame();
}

bool RendererFrontend::EndMainRenderpass()
{
    return RendererData.Backend->EndFrame();
}

//
// API
//

bool RendererFrontend::SetSceneViewViewportSize(uint32 Width, uint32 Height)
{
    return RendererData.Backend->SetSceneViewViewportSize(Width, Height);
}

Handle<Texture> RendererFrontend::GetSceneViewTexture()
{
    return RendererData.Backend->GetSceneViewTexture();
}

void RendererFrontend::CreateRenderPipeline(Shader* Shader, int PassIndex)
{
    RendererData.Backend->CreateRenderPipeline(Shader, PassIndex);
}

void RendererFrontend::DestroyRenderPipeline(Shader* Shader)
{
    RendererData.Backend->DestroyRenderPipeline(Shader);
}

void RendererFrontend::CreateMaterial(Material* Material)
{
    RendererData.Backend->CreateMaterial(Material);
}

void RendererFrontend::DestroyMaterial(Material* Instance)
{
    RendererData.Backend->DestroyMaterial(Instance);
}

void RendererFrontend::UseShader(const Shader* Shader)
{
    RendererData.Backend->UseShader(Shader);
}

void RendererFrontend::SetUniform(Shader* ActiveShader, const ShaderUniform& Uniform, void* Value, bool Invalidate)
{
    RendererData.Backend->SetUniform(ActiveShader, Uniform, Value, Invalidate);
}

void RendererFrontend::DrawGeometry(uint32 GeometryID)
{
    RendererData.Backend->DrawGeometryData(GeometryID);
}

void RendererFrontend::ApplyGlobalShaderProperties(Shader* ActiveShader)
{
    RendererData.Backend->ApplyGlobalShaderProperties(ActiveShader);
}

void RendererFrontend::ApplyInstanceShaderProperties(Shader* ActiveShader)
{
    RendererData.Backend->ApplyInstanceShaderProperties(ActiveShader);
}

bool RendererFrontend::CreateGeometry(
    Geometry*    Geometry,
    uint32       VertexCount,
    const void*  Vertices,
    uint32       VertexSize,
    uint32       IndexCount,
    const void*  Indices,
    const uint32 IndexSize
)
{
    return RendererData.Backend->CreateGeometry(Geometry, VertexCount, Vertices, VertexSize, IndexCount, Indices, IndexSize);
}

void RendererFrontend::DestroyGeometry(Geometry* Geometry)
{
    RendererData.Backend->DestroyGeometry(Geometry);
}

}