#include "kraft_renderer_frontend.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_asserts.h"
#include "core/kraft_engine.h"
#include "renderer/kraft_renderer_backend.h"
#include "renderer/shaderfx/kraft_shaderfx_types.h"
#include "renderer/kraft_camera.h"

#include "systems/kraft_shader_system.h"
#include "systems/kraft_material_system.h"

#include <renderer/kraft_resource_manager.h>

namespace kraft::renderer
{

RendererFrontend* Renderer = nullptr;

bool RendererFrontend::Init(EngineConfig* Config)
{
    Renderer = this;
    Type = Config->RendererBackend;
    BackendMemory = MallocBlock(sizeof(RendererBackend));
    Backend = (RendererBackend*)BackendMemory.Data;
    this->Renderables.reserve(1024);

    KASSERTM(Type != RendererBackendType::RENDERER_BACKEND_TYPE_NONE, "No renderer backend specified");

    if (!CreateBackend(Type, Backend))
    {
        return false;
    }

    if (!Backend->Init(Config))
    {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return false;
    }

    return true;
}

bool RendererFrontend::Shutdown()
{
    DestroyBackend(Backend);

    FreeBlock(BackendMemory);
    Backend = nullptr;

    return true;
}

void RendererFrontend::OnResize(int width, int height)
{
    if (Backend)
    {
        Backend->OnResize(width, height);
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

    Backend->PrepareFrame();
    Backend->BeginSceneView();
    for (auto It = this->Renderables.vbegin(); It != this->Renderables.vend(); It++)
    {
        auto& Objects = *It;
        uint64 Count = Objects.Size();
        if (!Count) continue;
        ShaderSystem::Bind(Objects[0].MaterialInstance->Shader);
        ShaderSystem::ApplyGlobalProperties(ProjectionMatrix, ViewMatrix);
        for (int i = 0; i < Count; i++)
        {
            Renderable Object = Objects[i];
            ShaderSystem::SetMaterialInstance(Object.MaterialInstance);

            MaterialSystem::ApplyInstanceProperties(Object.MaterialInstance);
            MaterialSystem::ApplyLocalProperties(Object.MaterialInstance, Object.ModelMatrix);

            Backend->DrawGeometryData(Object.GeometryID);
        }
        ShaderSystem::Unbind();

        Objects.Clear();
    }
    Backend->EndSceneView();

    return true;

//     if (Backend->BeginFrame())
//     {
// #if KRAFT_IMGUI_ENABLED
//         ImGuiRenderer.BeginFrame();
//         ImGuiRenderer.RenderWidgets();
//         ImGuiRenderer.EndFrame();
// #endif

//         if (!Backend->EndFrame())
//         {
//             KERROR("[RendererFrontend::DrawFrame]: End frame failed!");
//             return false;
//         }

// #if KRAFT_IMGUI_ENABLED
//         ImGuiRenderer.EndFrameUpdatePlatformWindows();
// #endif

//         ResourceManager::Ptr->EndFrame(Frame);

//         Frame++;

//         return true;
//     }

//     return false;
}

bool RendererFrontend::AddRenderable(Renderable Object)
{
    ResourceID Key = Object.MaterialInstance->Shader->ID;
    if (!this->Renderables.has(Key))
    {
        this->Renderables[Key] = Array<Renderable>();
        this->Renderables[Key].Reserve(1024);
    }

    this->Renderables[Key].Push(Object);

    return true;
}

// 
// API
//

bool RendererFrontend::SetSceneViewViewportSize(uint32 Width, uint32 Height)
{
    return Backend->SetSceneViewViewportSize(Width, Height);
}

Handle<Texture> RendererFrontend::GetSceneViewTexture()
{
    return Backend->GetSceneViewTexture();
}

void RendererFrontend::CreateRenderPipeline(Shader* Shader, int PassIndex)
{
    Backend->CreateRenderPipeline(Shader, PassIndex);
}

void RendererFrontend::DestroyRenderPipeline(Shader* Shader)
{
    Backend->DestroyRenderPipeline(Shader);
}

void RendererFrontend::CreateMaterial(Material* Material)
{
    Backend->CreateMaterial(Material);
}

void RendererFrontend::DestroyMaterial(Material* Instance)
{
    Backend->DestroyMaterial(Instance);
}

void RendererFrontend::UseShader(const Shader* Shader)
{
    Backend->UseShader(Shader);
}

void RendererFrontend::SetUniform(Shader* ActiveShader, const ShaderUniform& Uniform, void* Value, bool Invalidate)
{
    Backend->SetUniform(ActiveShader, Uniform, Value, Invalidate);
}

void RendererFrontend::DrawGeometry(uint32 GeometryID)
{
    Backend->DrawGeometryData(GeometryID);
}

void RendererFrontend::ApplyGlobalShaderProperties(Shader* ActiveShader)
{
    Backend->ApplyGlobalShaderProperties(ActiveShader);
}

void RendererFrontend::ApplyInstanceShaderProperties(Shader* ActiveShader)
{
    Backend->ApplyInstanceShaderProperties(ActiveShader);
}

bool RendererFrontend::CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize)
{
    return Backend->CreateGeometry(Geometry, VertexCount, Vertices, VertexSize, IndexCount, Indices, IndexSize);
}

void RendererFrontend::DestroyGeometry(Geometry* Geometry)
{
    Backend->DestroyGeometry(Geometry);
}

}