#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

namespace VulkanRendererBackend
{
    bool Init();
    bool Shutdown();
    bool BeginFrame(float64 deltaTime);
    bool EndFrame(float64 deltaTime);
    void OnResize(int width, int height);

    void CreateVulkanSurface();

#ifdef KRAFT_DEBUG
    VkBool32 DebugUtilsMessenger(
        VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
        void*                                            pUserData);
#endif
};

}