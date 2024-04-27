#include "kraft_renderer_frontend.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_application.h"
#include "core/kraft_asserts.h"
#include "renderer/kraft_renderer_backend.h"
#include "renderer/kraft_renderer_imgui.h"
#include "renderer/shaderfx/kraft_shaderfx_types.h"

#include "systems/kraft_shader_system.h"
#include "systems/kraft_material_system.h"

#define KRAFT_IMGUI_ENABLED 1

namespace kraft::renderer
{

RendererFrontend* Renderer = nullptr;

bool RendererFrontend::Init(ApplicationConfig* config)
{
    Renderer = this;
    Type = config->RendererBackend;
    BackendMemory = MallocBlock(sizeof(RendererBackend));
    Backend = (RendererBackend*)BackendMemory.Data;
    this->Renderables.reserve(1024);

    KASSERTM(Type != RendererBackendType::RENDERER_BACKEND_TYPE_NONE, "No renderer backend specified");

    if (!CreateBackend(Type, Backend))
    {
        return false;
    }

    if (!Backend->Init(config))
    {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return false;
    }

    ImGuiRenderer.Init(config);

    return true;
}

bool RendererFrontend::Shutdown()
{
    ImGuiRenderer.Destroy();
    bool ret = Backend->Shutdown();

    FreeBlock(BackendMemory);
    Backend = nullptr;

    return ret;
}

void RendererFrontend::OnResize(int width, int height)
{
    if (Backend)
    {
        Backend->OnResize(width, height);
        ImGuiRenderer.OnResize(width, height);
    }
    else
    {
        KERROR("[RendererFrontend::OnResize]: No backend!");
    }
}

bool RendererFrontend::DrawFrame(RenderPacket* Packet)
{
    if (Backend->BeginFrame(Packet->DeltaTime))
    {
        for (auto It = this->Renderables.vbegin(); It != this->Renderables.vend(); It++)
        {
            auto& Objects = *It;
            uint64 Count = Objects.Size();
            if (!Count) continue;
            ShaderSystem::Bind(Objects[0].MaterialInstance->Shader);
            ShaderSystem::ApplyGlobalProperties(Packet->ProjectionMatrix, Packet->ViewMatrix);
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

#if KRAFT_IMGUI_ENABLED
        ImGuiRenderer.BeginFrame(Packet->DeltaTime);
        ImGuiRenderer.RenderWidgets();
        ImGuiRenderer.EndFrame();
#endif

        if (!Backend->EndFrame(Packet->DeltaTime))
        {
            KERROR("[RendererFrontend::DrawFrame]: End frame failed!");
            return false;
        }

#if KRAFT_IMGUI_ENABLED
        ImGuiRenderer.EndFrameUpdatePlatformWindows();
#endif

        return true;
    }

    return false;
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

void RendererFrontend::CreateRenderPipeline(Shader* Shader, int PassIndex)
{
    Backend->CreateRenderPipeline(Shader, PassIndex);
}

void RendererFrontend::DestroyRenderPipeline(Shader* Shader)
{
    Backend->DestroyRenderPipeline(Shader);
}

void RendererFrontend::CreateTexture(uint8* data, Texture* out)
{
    Backend->CreateTexture(data, out);
}

void RendererFrontend::DestroyTexture(Texture* texture)
{
    Backend->DestroyTexture(texture);
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