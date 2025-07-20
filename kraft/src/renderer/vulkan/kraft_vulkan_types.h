#pragma once

#ifndef VOLK_H_
#error "Vulkan headers not found. Did you forget to include volk.h before this file?"
#endif

#include <containers/kraft_array.h>
#include <core/kraft_core.h>
#include <math/kraft_math.h>
#include <renderer/kraft_renderer_types.h>
#include <shaders/includes/kraft_shader_includes.h>

#define KRAFT_VK_CHECK(expression)                                                                                                                                                                     \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        KRAFT_ASSERT(expression == VK_SUCCESS)                                                                                                                                                         \
    } while (0)

#define KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES 3
#define KRAFT_VULKAN_MAX_GEOMETRIES       1024
#define KRAFT_VULKAN_MAX_MATERIALS        1024
#define KRAFT_VULKAN_MAX_BINDINGS         32

namespace kraft {
struct ArenaAllocator;
}

namespace kraft::renderer {

class VulkanResourceManager;

struct VulkanDescriptorState
{
    uint8 Generations[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
};

struct VulkanMaterialData
{
    uint64                InstanceUBOOffset;
    VkDescriptorSet       LocalDescriptorSets[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
    VulkanDescriptorState DescriptorStates[KRAFT_VULKAN_MAX_BINDINGS];
};

//
// Resources
//

struct VulkanTexture
{
    uint32         Width;
    uint32         Height;
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
    uint32 ID;
    uint32 IndexSize;
    uint32 IndexCount;
    uint32 IndexBufferOffset;
    uint32 VertexSize;
    uint32 VertexCount;
    uint32 VertexBufferOffset;
};

struct VulkanBuffer
{
    VkBuffer              Handle;
    uint64                Size;
    VkDeviceMemory        Memory;
    VkBufferUsageFlags    UsageFlags;
    bool                  IsLocked;
    int32                 MemoryIndex;
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
    uint32                             AttributeCount;
    VkDescriptorSetLayout*             DescriptorSetLayouts;
    uint32                             DescriptorSetLayoutCount;
    VkPipelineShaderStageCreateInfo*   ShaderStages;
    uint32                             ShaderStageCount;
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
    uint32        Width;
    uint32        Height;
    uint32        AttachmentCount;
    VkImageView*  Attachments;
};

struct VulkanRenderPass
{
    VkRenderPass          Handle;
    Vec4f                 Rect;
    Vec4f                 Color;
    float32               Depth;
    uint32                Stencil;
    VulkanRenderPassState State;
    VulkanFramebuffer     Framebuffer;
};

struct VulkanQueueFamilyInfo
{
    int32 GraphicsQueueIndex;
    int32 ComputeQueueIndex;
    int32 TransferQueueIndex;
    int32 PresentQueueIndex;
};

struct VulkanSwapchainSupportInfo
{
    uint32                   FormatCount;
    uint32                   PresentModeCount;
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
    uint8              CurrentFrame;
    uint8              MaxFramesInFlight;
    uint32             ImageCount;
    Handle<Texture>    DepthAttachment;
    VulkanFramebuffer* Framebuffers;
};

struct VulkanContext
{
    RendererOptions*       Options;
    VkInstance             Instance;
    VkAllocationCallbacks* AllocationCallbacks;
    VulkanPhysicalDevice   PhysicalDevice;
    VulkanLogicalDevice    LogicalDevice;
    VkSurfaceKHR           Surface;
    uint32                 FramebufferWidth;
    uint32                 FramebufferHeight;
    VulkanSwapchain        Swapchain;
    VulkanRenderPass       MainRenderPass;
    Handle<CommandPool>    GraphicsCommandPool;
    Handle<CommandBuffer>  GraphicsCommandBuffers[3];
    Handle<CommandBuffer>  ActiveCommandBuffer;
    VulkanFence*           WaitFences;
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
    uint32        CurrentSwapchainImageIndex;

#ifdef KRAFT_RENDERER_DEBUG
    VkDebugUtilsMessengerEXT DebugMessenger;
    void (*SetObjectName)(uint64 Object, VkObjectType ObjectType, const char* Name);
#endif

    ArenaAllocator* Arena;

    Handle<Buffer>     VertexBuffer;
    Handle<Buffer>     IndexBuffer;
    uint32             CurrentVertexBufferOffset;
    uint32             CurrentIndexBufferOffset;
    VulkanGeometryData Geometries[KRAFT_VULKAN_MAX_GEOMETRIES];
    VulkanMaterialData Materials[KRAFT_VULKAN_MAX_MATERIALS];

    // Global Data
    VkDescriptorPool       GlobalDescriptorPool;
    VkDescriptorSetLayout  DescriptorSetLayouts[16];
    VkDescriptorSet        GlobalDescriptorSets[16][KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
    VkDescriptorSet        GlobalTexturesDescriptorSet;
    uint8                  DescriptorSetLayoutsCount = 0;
    Handle<Buffer>         GlobalUniformBuffer = Handle<Buffer>::Invalid();
    Handle<Buffer>         GlobalPickingDataBuffer = Handle<Buffer>::Invalid();
    void*                  GlobalUniformBufferMemory = 0;
    void*                  GlobalPickingDataBufferMemory = 0;
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
    uint16       DeviceExtensionsCount;
    const char** DeviceExtensions;
};

struct VulkanShaderResources
{
    VkDescriptorSetLayout LocalDescriptorSetLayout = 0;
    Handle<Buffer>        UniformBuffer = Handle<Buffer>::Invalid();
    void*                 UniformBufferMemory = 0;

    // This is used to figure out where in memory the next material
    // uniform data will be stored
    struct Slot
    {
        bool Used;
    };
    Slot UsedSlots[128];
};

struct VulkanShader
{
    VkPipeline            Pipeline;
    VkPipelineLayout      PipelineLayout;
    VulkanShaderResources ShaderResources;
};

} // namespace kraft::renderer