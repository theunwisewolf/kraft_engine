#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

struct ApplicationConfig;

namespace VulkanRendererBackend
{
    bool Init(ApplicationConfig* config);
    bool Shutdown();
    bool BeginFrame(float64 deltaTime);
    bool EndFrame(float64 deltaTime);
    void OnResize(int width, int height);

    void CreateTexture(uint8* data, Texture* texture);
    void DestroyTexture(Texture* texture);

#ifdef KRAFT_DEBUG
    VkBool32 DebugUtilsMessenger(
        VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
        void*                                            pUserData);
#endif
};

}