#include "kraft_vulkan_surface.h"

#include "platform/kraft_platform.h"
#include "platform/kraft_window.h"

// #define VK_USE_PLATFORM_METAL_EXT
// #include <volk/volk.h>

#include <GLFW/glfw3.h>

namespace kraft::renderer
{

void CreateVulkanSurface(VulkanContext* context)
{
    GLFWwindow* window = Platform::GetWindow().PlatformWindowHandle;
    KRAFT_VK_CHECK(glfwCreateWindowSurface(context->Instance, window, context->AllocationCallbacks, &context->Surface));

    KSUCCESS("[CreateVulkanSurface]: Successfully created VkSurface");
}

}