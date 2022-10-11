#pragma once

#include "core/kraft_core.h"
#include "core/kraft_asserts.h"
#include "math/kraft_math.h"

#include <vulkan/vulkan.h>

#define KRAFT_VK_CHECK(expression) \
    KRAFT_ASSERT(expression == VK_SUCCESS)

namespace kraft
{

struct VulkanFence
{
    VkFence Handle;
    bool    Signalled;
};

enum VulkanCommandBufferState
{
    VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED,
    VULKAN_COMMAND_BUFFER_STATE_READY,
    VULKAN_COMMAND_BUFFER_STATE_RECORDING,
    VULKAN_COMMAND_BUFFER_STATE_IN_RENDER_PASS,
    VULKAN_COMMAND_BUFFER_STATE_RECORDING_ENDED,
    VULKAN_COMMAND_BUFFER_STATE_SUBMITTED,
};

struct VulkanCommandBuffer
{
    VkCommandBuffer          Handle;
    VulkanCommandBufferState State;
};

enum VulkanRenderPassState
{
    VULKAN_RENDER_PASS_STATE_NOT_ALLOCATED,
    VULKAN_RENDER_PASS_STATE_READY,
    VULKAN_RENDER_PASS_STATE_RECORDING,
    VULKAN_RENDER_PASS_STATE_IN_RENDER_PASS,
    VULKAN_RENDER_PASS_STATE_RECORDING_ENDED,
    VULKAN_RENDER_PASS_STATE_SUBMITTED,
};

struct VulkanRenderPass
{
    VkRenderPass          Handle;
    Vec4f                 Rect;
    Vec4f                 Color;
    float32               Depth;
    uint32                Stencil;
    VulkanRenderPassState State;
};

struct VulkanFramebuffer
{
    VkFramebuffer       Handle;
    uint32              Width;
    uint32              Height;
    uint32              AttachmentCount;
    VulkanRenderPass*   RenderPass;
    VkImageView*        Attachments;
};

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
    VkCommandPool           GraphicsCommandPool;
};

struct VulkanSwapchain
{
    VkSwapchainKHR      Handle;
    VkSurfaceFormatKHR  ImageFormat;
    VkImage*            Images;
    VkImageView*        ImageViews;
    uint8               CurrentFrame;               
    uint8               MaxFramesInFlight;               
    uint32              ImageCount;
    VulkanImage         DepthAttachment;
    VulkanFramebuffer*  Framebuffers;
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
    VulkanRenderPass         MainRenderPass;
    VulkanCommandBuffer*     GraphicsCommandBuffers;
    VulkanFence*             WaitFences;
    VulkanFence**            InFlightImageToFenceMap;
    VkSemaphore*             PresentCompleteSemaphores;
    VkSemaphore*             RenderCompleteSemaphores;
    uint32                   CurrentSwapchainImageIndex;

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