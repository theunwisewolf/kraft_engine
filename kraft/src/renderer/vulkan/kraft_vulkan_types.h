#pragma once

#include "core/kraft_core.h"
#include "core/kraft_asserts.h"
#include "math/kraft_math.h"
#include "containers/kraft_array.h"

#include <vulkan/vulkan.h>

#define KRAFT_VK_CHECK(expression) \
    do { \
        KRAFT_ASSERT(expression == VK_SUCCESS) \
    } while (0)

#define KRAFT_VULKAN_MAX_GEOMETRIES         1024
#define KRAFT_VULKAN_MAX_BINDINGS           32

namespace kraft::renderer
{

struct VulkanBuffer
{
    VkBuffer                Handle;
    uint64                  Size;
    VkDeviceMemory          Memory;
    VkBufferUsageFlags      UsageFlags;
    bool                    IsLocked;
    int32                   MemoryIndex;
    VkMemoryPropertyFlags   MemoryPropertyFlags;
};

struct VulkanPipeline
{
    VkPipelineLayout Layout;
    VkPipeline       Handle;
};

struct VulkanPipelineDescription
{
    VkVertexInputAttributeDescription*  Attributes;
    uint32                              AttributeCount;
    VkDescriptorSetLayout*              DescriptorSetLayouts;
    uint32                              DescriptorSetLayoutCount;
    VkPipelineShaderStageCreateInfo*    ShaderStages;
    uint32                              ShaderStageCount;
    VkViewport                          Viewport;
    VkRect2D                            Scissor;
    bool                                IsWireframe;
};

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
    bool                                SupportsDeviceLocalHostVisible;
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
    uint8               CurrentFrame;               
    uint8               MaxFramesInFlight;               
    uint32              ImageCount;
    VulkanImage         DepthAttachment;
    VulkanFramebuffer*  Framebuffers;
};

//
// Resources
//

struct VulkanTexture
{
    VulkanImage Image;
    VkSampler   Sampler;
};

struct VulkanGeometryData
{
    uint32 ID;
    uint32 IndexSize;
    uint32 IndexCount;
    uint32 IndexBufferOffset;
    uint32 VertexSize;
    uint32 VertexCount;
    uint32 VertexBufferOffset;
};

//
// End resources
//

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
    VkCommandPool            GraphicsCommandPool;
    VulkanCommandBuffer*     GraphicsCommandBuffers;
    VulkanFence*             WaitFences;
    // If everything was perfect, this mapping below would not be needed
    // but, what I saw happening was on an M1 Mac, vkAcquireNextImageKHR
    // was returning an image for which the associated command buffers 
    // were still active. So it was just spitting errors. The fix was to
    // associate a fence with each image and before rendering begins,
    // just wait on the acquired image's corresponding fence. This works 
    // both on Mac and Windows. Maybe I am doing something wrong.
    VulkanFence**            InFlightImageToFenceMap;
    VkSemaphore*             ImageAvailableSemaphores;
    VkSemaphore*             RenderCompleteSemaphores;
    uint32                   CurrentSwapchainImageIndex;

#ifdef KRAFT_RENDERER_DEBUG
    VkDebugUtilsMessengerEXT DebugMessenger;
#endif

    VulkanBuffer             VertexBuffer;
    VulkanBuffer             IndexBuffer;
    uint32                   CurrentVertexBufferOffset;
    uint32                   CurrentIndexBufferOffset;
    VulkanGeometryData       Geometries[KRAFT_VULKAN_MAX_GEOMETRIES];
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

struct VulkanDescriptorState
{
    uint8 Generations[3];
};

struct VulkanShaderResources
{
    Array<VkDescriptorSetLayout> DescriptorSetLayouts;
    VkDescriptorSet              GlobalDescriptorSets[3];
    VulkanBuffer                 UniformBuffer;
    void*                        UniformBufferMemory;
    
    // This is used to figure out where in memory the next material
    // uniform data will be stored
    struct Slot { bool Used; };
    Slot                         UsedSlots[128];
};

struct VulkanShader
{
    VkPipeline             Pipeline;
    VkPipelineLayout       PipelineLayout;
    VulkanShaderResources  ShaderResources;
};

struct VulkanMaterialData
{
    uint64                 InstanceUBOOffset;
    Array<VkDescriptorSet> DescriptorSets;
    VulkanDescriptorState  DescriptorStates[KRAFT_VULKAN_MAX_BINDINGS];
};

}