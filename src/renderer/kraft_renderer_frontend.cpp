#include "kraft_renderer_frontend.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_application.h"
#include "core/kraft_asserts.h"
#include "renderer/kraft_renderer_backend.h"

namespace kraft
{

RendererFrontend* Renderer = nullptr;

bool RendererFrontend::Init(ApplicationConfig* config)
{
    Renderer = this;
    Type = config->RendererBackend;
    BackendMemory = MallocBlock(sizeof(RendererBackend));
    Backend = (RendererBackend*)BackendMemory.Data;
    ImGuiRenderer.Init();

    KASSERTM(Type != RendererBackendType::RENDERER_BACKEND_TYPE_NONE, "No renderer backend specified");

    if (!RendererCreateBackend(Type, Backend))
    {
        return false;
    }

    if (!Backend->Init(config))
    {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return false;
    }

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
        ImGuiRenderer.RenderWidgets();

        if (!Backend->EndFrame(packet->DeltaTime))
        {
            KERROR("[RendererFrontend::DrawFrame]: End frame failed!");
            return false;
        }

        return true;
    }

    return false;
}

// 
// API
//
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