#include "kraft_renderer_backend.h"

#include <renderer/vulkan/kraft_vulkan_backend.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>
#include <renderer/kraft_resource_manager.h>

namespace kraft::renderer
{

bool CreateBackend(RendererBackendType type, RendererBackend* backend)
{
    if (type == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        backend->Init       = VulkanRendererBackend::Init;
        backend->Shutdown   = VulkanRendererBackend::Shutdown;
        backend->PrepareFrame = VulkanRendererBackend::PrepareFrame;
        backend->BeginFrame = VulkanRendererBackend::BeginFrame;
        backend->EndFrame   = VulkanRendererBackend::EndFrame;
        backend->OnResize   = VulkanRendererBackend::OnResize;
        backend->BeginSceneView = VulkanRendererBackend::BeginSceneView;
        backend->EndSceneView = VulkanRendererBackend::EndSceneView;
        backend->OnSceneViewViewportResize = VulkanRendererBackend::OnSceneViewViewportResize;
        backend->GetSceneViewTexture = VulkanRendererBackend::GetSceneViewTexture;
        backend->SetSceneViewViewportSize = VulkanRendererBackend::SetSceneViewViewportSize;
        
        backend->CreateRenderPipeline = VulkanRendererBackend::CreateRenderPipeline;
        backend->DestroyRenderPipeline = VulkanRendererBackend::DestroyRenderPipeline;
        backend->UseShader = VulkanRendererBackend::UseShader;
        backend->SetUniform = VulkanRendererBackend::SetUniform;
        backend->ApplyGlobalShaderProperties = VulkanRendererBackend::ApplyGlobalShaderProperties;
        backend->ApplyInstanceShaderProperties = VulkanRendererBackend::ApplyInstanceShaderProperties;
        backend->CreateMaterial = VulkanRendererBackend::CreateMaterial;
        backend->DestroyMaterial = VulkanRendererBackend::DestroyMaterial;
        backend->CreateGeometry = VulkanRendererBackend::CreateGeometry;
        backend->DrawGeometryData = VulkanRendererBackend::DrawGeometryData;
        backend->DestroyGeometry = VulkanRendererBackend::DestroyGeometry;

        ResourceManager::Ptr = new VulkanResourceManager();

        return true;
    }
    else if (type == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {
    }

    return false;
}

void DestroyBackend(RendererBackend* Backend)
{
    delete ResourceManager::Ptr;

    bool ret = Backend->Shutdown();
    MemZero(Backend, sizeof(RendererBackend));
}

}