#include "kraft_renderer_frontend.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_application.h"
#include "core/kraft_asserts.h"
#include "renderer/kraft_renderer_backend.h"
#include "renderer/kraft_renderer_imgui.h"
#include "renderer/shaderfx/kraft_shaderfx_types.h"

namespace kraft::renderer
{

RendererFrontend* Renderer = nullptr;

bool RendererFrontend::Init(ApplicationConfig* config)
{
    Renderer = this;
    Type = config->RendererBackend;
    BackendMemory = MallocBlock(sizeof(RendererBackend));
    Backend = (RendererBackend*)BackendMemory.Data;

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
        ImGuiRenderer.Backend->BeginFrame(Packet->DeltaTime);
        ImGuiRenderer.RenderWidgets();
        ImGuiRenderer.Backend->EndFrame(Packet->DeltaTime);

        // Array<GeometryRenderData>& Geometries = Packet->Geometries;
        // Array<Material*>& MaterialInstances = Packet->MaterialInstances;
        // uint64 Count = Geometries.Length;
        // for (int i = 0; i < Count; i++)
        // {

        // }

        if (!Backend->EndFrame(Packet->DeltaTime))
        {
            KERROR("[RendererFrontend::DrawFrame]: End frame failed!");
            return false;
        }

        ImGuiRenderer.EndFrameUpdatePlatformWindows();
        return true;
    }

    return false;
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

void RendererFrontend::CreateMaterial(Material* material)
{
    Backend->CreateMaterial(material);
}

void RendererFrontend::DestroyMaterial(Material* material)
{
    Backend->DestroyMaterial(material);
}

void RendererFrontend::UseShader(const Shader* Shader)
{
    Backend->UseShader(Shader);
}

void RendererFrontend::SetUniform(Shader* Shader, const ShaderUniform& Uniform, void* Value, bool Invalidate)
{
    Backend->SetUniform(Shader, Uniform, Value, Invalidate);
}

void RendererFrontend::DrawGeometry(GeometryRenderData Data)
{
    Backend->DrawGeometryData(Data);
}

void RendererFrontend::ApplyGlobalShaderProperties(Shader* Shader)
{
    Backend->ApplyGlobalShaderProperties(Shader);
}

void RendererFrontend::ApplyInstanceShaderProperties(Shader* Shader)
{
    Backend->ApplyInstanceShaderProperties(Shader);
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