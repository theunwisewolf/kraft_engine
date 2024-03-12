#include "kraft_renderer_backend.h"

#include "renderer/vulkan/kraft_vulkan_backend.h"

namespace kraft::renderer
{

bool CreateBackend(RendererBackendType type, RendererBackend* backend)
{
    if (type == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        backend->Init       = VulkanRendererBackend::Init;
        backend->Shutdown   = VulkanRendererBackend::Shutdown;
        backend->BeginFrame = VulkanRendererBackend::BeginFrame;
        backend->EndFrame   = VulkanRendererBackend::EndFrame;
        backend->OnResize   = VulkanRendererBackend::OnResize;
        
        backend->CreateRenderPipeline = VulkanRendererBackend::CreateRenderPipeline;
        backend->CreateTexture  = VulkanRendererBackend::CreateTexture;
        backend->DestroyTexture = VulkanRendererBackend::DestroyTexture;
        backend->CreateMaterial = VulkanRendererBackend::CreateMaterial;
        backend->DestroyMaterial = VulkanRendererBackend::DestroyMaterial;

        return true;
    }
    else if (type == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {
    }

    return false;
}

void DestroyBackend(RendererBackend* backend)
{
    backend->Init       = 0;
    backend->Shutdown   = 0;
    backend->BeginFrame = 0;
    backend->EndFrame   = 0;
    backend->OnResize   = 0;
    
    backend->CreateTexture = 0;
    backend->DestroyTexture = 0;
    backend->CreateMaterial = 0;
    backend->DestroyMaterial = 0;
}

}