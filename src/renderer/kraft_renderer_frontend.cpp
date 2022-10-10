#include "kraft_renderer_frontend.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_application.h"
#include "core/kraft_asserts.h"
#include "renderer/kraft_renderer_backend.h"

namespace kraft
{

RendererFrontend* RendererFrontend::I = nullptr;

bool RendererFrontend::Init(ApplicationConfig* config)
{
    Type = config->RendererBackend;
    BackendMemory = MallocBlock(sizeof(RendererBackend));
    Backend = (RendererBackend*)BackendMemory.Data;
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
    if (Backend->BeginFrame(packet->deltaTime))
    {
        if (!Backend->EndFrame(packet->deltaTime))
        {
            KERROR("[RendererFrontend::DrawFrame]: End frame failed!");
            return false;
        }

        return true;
    }

    return false;
}

}