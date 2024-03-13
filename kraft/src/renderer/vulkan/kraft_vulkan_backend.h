#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

struct ApplicationConfig;

namespace renderer
{

struct ShaderEffect;

namespace VulkanRendererBackend
{
    bool Init(ApplicationConfig* config);
    bool Shutdown();
    bool BeginFrame(float64 deltaTime);
    bool EndFrame(float64 deltaTime);
    void OnResize(int width, int height);

    RenderPipeline CreateRenderPipeline(const ShaderEffect& Effect, int PassIndex);
    void AllocateResources(RenderResource& RenderResources);

    // Textures
    void CreateTexture(uint8* data, Texture* texture);
    void DestroyTexture(Texture* texture);

    // Materials
    void CreateMaterial(Material* material);
    void DestroyMaterial(Material* material);

    // Geometry
    void DrawGeometryData(GeometryRenderData Data);
    bool CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 IndexCount, const void* Indices);
    void DestroyGeometry(Geometry* Geometry);

    VulkanContext* GetContext();
#ifdef KRAFT_RENDERER_DEBUG
    VkBool32 DebugUtilsMessenger(
        VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
        void*                                            pUserData);
#endif
};

}

}