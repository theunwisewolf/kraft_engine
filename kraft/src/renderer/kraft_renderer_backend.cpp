#include "kraft_renderer_backend.h"

#include <math/kraft_math.h>
#include <containers/kraft_array.h>
#include <renderer/kraft_resource_manager.h>
#include <renderer/vulkan/kraft_vulkan_backend.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>

#include <renderer/kraft_renderer_types.h>

namespace kraft::renderer {

bool CreateBackend(RendererBackendType type, RendererBackend* backend)
{
    if (type == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        backend->Init = VulkanRendererBackend::Init;
        backend->Shutdown = VulkanRendererBackend::Shutdown;
        backend->PrepareFrame = VulkanRendererBackend::PrepareFrame;
        backend->BeginFrame = VulkanRendererBackend::BeginFrame;
        backend->EndFrame = VulkanRendererBackend::EndFrame;
        backend->OnResize = VulkanRendererBackend::OnResize;

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

        // Render Passes
        backend->BeginRenderPass = VulkanRendererBackend::BeginRenderPass;
        backend->EndRenderPass = VulkanRendererBackend::EndRenderPass;

        return true;
    }
    else if (type == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {
    }

    return false;
}

void DestroyBackend(RendererBackend* Backend)
{
    bool ret = Backend->Shutdown();
    MemZero(Backend, sizeof(RendererBackend));
}

}