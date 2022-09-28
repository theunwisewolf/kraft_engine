#include "kraft_renderer_frontend.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "renderer/kraft_renderer_backend.h"

namespace kraft
{

RendererFrontend* RendererFrontend::I = nullptr;

bool RendererFrontend::Init()
{
    BackendMemory = MallocBlock(sizeof(RendererBackend));
    Backend = (RendererBackend*)BackendMemory.Data;

    if (!RendererCreateBackend(Type, Backend))
    {
        return false;
    }

    if (!Backend->Init())
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
    Backend->OnResize(width, height);
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