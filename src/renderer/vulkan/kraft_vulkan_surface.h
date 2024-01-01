#pragma once

#define VK_USE_PLATFORM_METAL_EXT
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "platform/kraft_platform.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft 
{

void CreateVulkanSurface(VulkanContext* context)
{
    GLFWwindow* window;

#if defined(KRAFT_PLATFORM_WINDOWS)
    Win32PlatformState* State = (Win32PlatformState*)Platform::InternalState;
#elif defined(KRAFT_PLATFORM_MACOS)
    MacOSPlatformState* State = (MacOSPlatformState*)Platform::InternalState;
#elif defined(KRAFT_PLATFORM_LINUX)
    LinuxPlatformState* State = (LinuxPlatformState*)Platform::InternalState;
#endif

    window = State->Window.PlatformWindowHandle;
    KRAFT_VK_CHECK(glfwCreateWindowSurface(context->Instance, window, context->AllocationCallbacks, &context->Surface));

    KSUCCESS(TEXT("[CreateVulkanSurface]: Successfully created VkSurface"));
}

}