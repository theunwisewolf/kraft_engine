#pragma once

#ifndef VOLK_H_
#error "Vulkan headers not found. Did you forget to include volk.h before this file?"
#endif

#include <containers/kraft_array.h>
#include <core/kraft_base_includes.h>
#include <renderer/kraft_renderer_types.h>
#include <shaders/includes/kraft_shader_includes.h>

#ifndef KRAFT_ENABLE_VK_DYNAMIC_RENDERING
#define KRAFT_ENABLE_VK_DYNAMIC_RENDERING 1
#endif

#define KRAFT_VK_CHECK(expression)                                                                                                                                                                     \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        KRAFT_ASSERT(expression == VK_SUCCESS)                                                                                                                                                         \
    } while (0)

#define KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES           3
#define KRAFT_VULKAN_MAX_GEOMETRIES                 1024
#define KRAFT_VULKAN_MAX_MATERIALS                  1024
#define KRAFT_VULKAN_MAX_BINDINGS                   32
#define KRAFT_VULKAN_NUM_INBUILT_DESCRIPTOR_SETS    3
#define KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS     4
#define KRAFT_VULKAN_MAX_DESCRIPTOR_SETS_PER_SHADER KRAFT_VULKAN_NUM_INBUILT_DESCRIPTOR_SETS + KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS
#define KRAFT_VULKAN_MAX_BINDINGS_PER_SET           16

namespace kraft {
struct ArenaAllocator;
}

namespace kraft::r {

class VulkanResourceManager;

struct VulkanDescriptorState
{
    u8 Generations[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
};

struct VulkanMaterialData
{
    u64                   InstanceUBOOffset;
    VkDescriptorSet       LocalDescriptorSets[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
    VulkanDescriptorState DescriptorStates[KRAFT_VULKAN_MAX_BINDINGS];
};

//
// Resources
//

struct VulkanTexture
{
    u32            Width;
    u32            Height;
    VkImage        Image;
    VkImageView    View;
    VkDeviceMemory Memory;

    VulkanTexture() : Width(0), Height(0), Image(0), View(0), Memory(0)
    {}
};

struct VulkanTextureSampler
{
    VkSampler Sampler;
};

struct VulkanGeometryData
{
    u32 ID;
    u32 IndexSize;
    u32 IndexCount;
    u32 IndexBufferOffset;
    u32 VertexSize;
    u32 VertexCount;
    u32 VertexBufferOffset;
};

struct VulkanBuffer
{
    VkBuffer              Handle;
    uint64                Size;
    VkDeviceMemory        Memory;
    VkBufferUsageFlags    UsageFlags;
    bool                  IsLocked;
    i32                   MemoryIndex;
    VkMemoryPropertyFlags MemoryPropertyFlags;
};

//
// End resources
//

struct VulkanPipeline
{
    VkPipelineLayout Layout;
    VkPipeline       Handle;
};

struct VulkanPipelineDescription
{
    VkVertexInputAttributeDescription* Attributes;
    u32                                AttributeCount;
    VkDescriptorSetLayout*             DescriptorSetLayouts;
    u32                                DescriptorSetLayoutCount;
    VkPipelineShaderStageCreateInfo*   ShaderStages;
    u32                                ShaderStageCount;
    VkViewport                         Viewport;
    VkRect2D                           Scissor;
    bool                               IsWireframe;
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
    VkCommandBuffer          Resource;
    VkCommandPool            Pool;
    VulkanCommandBufferState State;
};

struct VulkanCommandPool
{
    VkCommandPool Resource;
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

struct VulkanFramebuffer
{
    VkFramebuffer Handle;
    u32           Width;
    u32           Height;
    u32           AttachmentCount;
    VkImageView*  Attachments;
};

struct VulkanRenderPass
{
    VkRenderPass          Handle;
    Vec4f                 Rect;
    Vec4f                 Color;
    f32                   Depth;
    u32                   Stencil;
    VulkanRenderPassState State;
    VulkanFramebuffer     Framebuffer;
};

struct VulkanQueueFamilyInfo
{
    i32 GraphicsQueueIndex;
    i32 ComputeQueueIndex;
    i32 TransferQueueIndex;
    i32 PresentQueueIndex;
};

struct VulkanSwapchainSupportInfo
{
    u32                      FormatCount;
    u32                      PresentModeCount;
    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    VkSurfaceFormatKHR*      Formats;
    VkPresentModeKHR*        PresentModes;
};

struct VulkanPhysicalDevice
{
    VkPhysicalDevice                 Handle;
    VkPhysicalDeviceProperties       Properties;
    VkPhysicalDeviceFeatures         Features;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
    VulkanQueueFamilyInfo            QueueFamilyInfo;
    VulkanSwapchainSupportInfo       SwapchainSupportInfo;
    VkFormat                         DepthBufferFormat;
    bool                             SupportsDeviceLocalHostVisible;
};

struct VulkanLogicalDevice
{
    VulkanPhysicalDevice PhysicalDevice;
    VkDevice             Handle;
    VkQueue              GraphicsQueue;
    VkQueue              ComputeQueue;
    VkQueue              TransferQueue;
    VkQueue              PresentQueue;
};

struct VulkanSwapchain
{
    VkSwapchainKHR     Resource;
    VkSurfaceFormatKHR ImageFormat;
    VkImage            Images[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
    VkImageView        ImageViews[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
    u8                 CurrentFrame;
    u8                 MaxFramesInFlight;
    u32                ImageCount;
    VulkanFramebuffer* Framebuffers;

    // Depending on the MSAASampleCount, this will either be MSAA or 1-Sample
    Handle<Texture>    DepthAttachment;

    // MSAA render targets
    Handle<Texture>       MSAAColorAttachment;
    VkSampleCountFlagBits MSAASampleCount = VK_SAMPLE_COUNT_1_BIT;
};

struct VulkanContext
{
    RendererOptions*       Options;
    VkInstance             Instance;
    VkAllocationCallbacks* AllocationCallbacks;
    VulkanPhysicalDevice   PhysicalDevice;
    VulkanLogicalDevice    LogicalDevice;
    VkSurfaceKHR           Surface;
    u32                    FramebufferWidth;
    u32                    FramebufferHeight;
    VulkanSwapchain        Swapchain;
#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
#else
    VulkanRenderPass MainRenderPass;
#endif
    Handle<CommandPool>   GraphicsCommandPool;
    Handle<CommandBuffer> GraphicsCommandBuffers[3];
    Handle<CommandBuffer> ActiveCommandBuffer;
    VulkanFence*          WaitFences;
    // VkCommandPool          GraphicsCommandPool;
    // If everything was perfect, this mapping below would not be needed
    // but, what I saw happening was on an M1 Mac, vkAcquireNextImageKHR
    // was returning an image for which the associated command buffers
    // were still active. So it was just spitting errors. The fix was to
    // associate a fence with each image and before rendering begins,
    // just wait on the acquired image's corresponding fence. This works
    // both on Mac and Windows. Maybe I am doing something wrong.
    VulkanFence** InFlightImageToFenceMap;
    VkSemaphore*  ImageAvailableSemaphores;
    VkSemaphore*  RenderCompleteSemaphores;
    u32           CurrentSwapchainImageIndex;

#ifdef KRAFT_RENDERER_DEBUG
    VkDebugUtilsMessengerEXT DebugMessenger;
    void (*SetObjectName)(uint64 Object, VkObjectType ObjectType, const char* Name);
#endif

    ArenaAllocator* Arena;

    Handle<Buffer>     VertexBuffer;
    Handle<Buffer>     IndexBuffer;
    u32                CurrentVertexBufferOffset;
    u32                CurrentIndexBufferOffset;
    VulkanGeometryData Geometries[KRAFT_VULKAN_MAX_GEOMETRIES];
    VulkanMaterialData Materials[KRAFT_VULKAN_MAX_MATERIALS];

    // Global Data
    VkDescriptorPool       GlobalDescriptorPool;
    VkDescriptorSetLayout  DescriptorSetLayouts[16];
    VkDescriptorSet        GlobalTexturesDescriptorSet;
    u8                     DescriptorSetLayoutsCount = 0;
    Handle<Buffer>         GlobalUniformBuffer = Handle<Buffer>::Invalid();
    void*                  GlobalUniformBufferMemory = 0;
    Handle<TextureSampler> DefaultTextureSampler;
};

struct VulkanPhysicalDeviceRequirements
{
    bool         Graphics;
    bool         Present;
    bool         Transfer;
    bool         Compute;
    bool         DiscreteGPU;
    bool         DepthBuffer;
    u16          DeviceExtensionsCount;
    const char** DeviceExtensions;
};

struct VulkanShader
{
    VkPipelineLayout      PipelineLayout;
    VkDescriptorSetLayout descriptor_set_layouts[KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS];
    VkDescriptorSet       descriptor_sets[KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS];

    u32         PipelineCount;
    VkPipeline* Pipelines; // One per variant
};

struct VulkanImageBarrierDescription
{
    VkPipelineStageFlags2 src_stage_mask;
    VkPipelineStageFlags2 dst_stage_mask;
    VkAccessFlags2        src_access_mask;
    VkAccessFlags2        dst_access_mask;
    VkImageLayout         old_layout;
    VkImageLayout         new_layout;
    VkImageAspectFlags    aspect_mask;
    u32                   base_mip_level;
    u32                   level_count;
};

} // namespace kraft::r