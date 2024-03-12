#include "kraft_renderer_frontend.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_application.h"
#include "core/kraft_asserts.h"
#include "renderer/kraft_renderer_backend.h"
#include "renderer/kraft_renderer_imgui.h"

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

bool RendererFrontend::DrawFrame(RenderPacket* packet)
{
    if (Backend->BeginFrame(packet->DeltaTime))
    {
        ImGuiRenderer.Backend->BeginFrame(packet->DeltaTime);
        ImGuiRenderer.RenderWidgets();
        ImGuiRenderer.Backend->EndFrame(packet->DeltaTime);

        if (!Backend->EndFrame(packet->DeltaTime))
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

RenderPipeline RendererFrontend::CreateRenderPipeline(const ShaderEffect& Effect, int PassIndex)
{
    return Backend->CreateRenderPipeline(Effect, PassIndex);
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

}