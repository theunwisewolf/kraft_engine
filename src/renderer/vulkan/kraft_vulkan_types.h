#pragma once

#include "core/kraft_core.h"
#include "core/kraft_asserts.h"

#include <vulkan/vulkan.h>

#define KRAFT_VK_CHECK(expression) \
    KRAFT_ASSERT(expression == VK_SUCCESS)

namespace kraft
{

struct VulkanImage
{
    uint32         Width;
    uint32         Height;
    VkImage        Handle;
    VkImageView    View;
    VkDeviceMemory Memory;
};

struct VulkanQueueFamilyInfo
{
    uint32 GraphicsQueueIndex;
    uint32 ComputeQueueIndex;
    uint32 TransferQueueIndex;
    uint32 PresentQueueIndex;
};

struct VulkanSwapchainSupportInfo
{
    uint32                      FormatCount;
    uint32                      PresentModeCount;
    VkSurfaceCapabilitiesKHR    SurfaceCapabilities;
    VkSurfaceFormatKHR*         Formats;
    VkPresentModeKHR*           PresentModes;
};

struct VulkanPhysicalDevice
{
    VkPhysicalDevice                    Handle;
    VkPhysicalDeviceProperties          Properties; 
    VkPhysicalDeviceFeatures            Features;
    VkPhysicalDeviceMemoryProperties    MemoryProperties;
    VulkanQueueFamilyInfo               QueueFamilyInfo;
    VulkanSwapchainSupportInfo          SwapchainSupportInfo;
    VkFormat                            DepthBufferFormat;
};

struct VulkanLogicalDevice
{
    VulkanPhysicalDevice    PhysicalDevice;
    VkDevice                Handle;
    VkQueue                 GraphicsQueue;
    VkQueue                 ComputeQueue;
    VkQueue                 TransferQueue;
    VkQueue                 PresentQueue;
};

struct VulkanSwapchain
{
    VkSwapchainKHR      Handle;
    VkSurfaceFormatKHR  ImageFormat;
    VkImage*            Images;
    VkImageView*        ImageViews;
    uint8               MaxFramesInFlight;               
    uint32              ImageCount;
    VulkanImage             DepthAttachment;
};

struct VulkanContext
{
    VkInstance               Instance;
    VkAllocationCallbacks*   AllocationCallbacks;
    VulkanPhysicalDevice     PhysicalDevice;
    VulkanLogicalDevice      LogicalDevice;
    VkSurfaceKHR             Surface;
    uint32                   FramebufferWidth;
    uint32                   FramebufferHeight;
    VulkanSwapchain          Swapchain;

#ifdef KRAFT_DEBUG
    VkDebugUtilsMessengerEXT DebugMessenger;
#endif
};

struct VulkanPhysicalDeviceRequirements
{
    bool Graphics;
    bool Present;
    bool Transfer;
    bool Compute;
    bool DiscreteGPU;
    bool DepthBuffer;

    const char** DeviceExtensionNames;
};

}