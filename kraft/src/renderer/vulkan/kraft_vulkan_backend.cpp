#include "kraft_vulkan_backend.h"

#include <volk/volk.h>
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <containers/kraft_carray.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_engine.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <renderer/kraft_resource_pool.inl>
#include <renderer/vulkan/kraft_vulkan_command_buffer.h>
#include <renderer/vulkan/kraft_vulkan_device.h>
#include <renderer/vulkan/kraft_vulkan_fence.h>
#include <renderer/vulkan/kraft_vulkan_framebuffer.h>
#include <renderer/vulkan/kraft_vulkan_helpers.h>
#include <renderer/vulkan/kraft_vulkan_image.h>
#include <renderer/vulkan/kraft_vulkan_pipeline.h>
#include <renderer/vulkan/kraft_vulkan_renderpass.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>
#include <renderer/vulkan/kraft_vulkan_shader.h>
#include <renderer/vulkan/kraft_vulkan_swapchain.h>
#include <shaders/includes/kraft_shader_includes.h>

#include <renderer/kraft_renderer_types.h>
#include <renderer/shaderfx/kraft_shaderfx_types.h>
#include <resources/kraft_resource_types.h>

namespace kraft::renderer {

#ifdef KRAFT_RENDERER_DEBUG
// PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag;
PFN_vkSetDebugUtilsObjectNameEXT KRAFT_vkSetDebugUtilsObjectNameEXT;
// PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin;
// PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd;
// PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert;
static VkBool32
DebugUtilsMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

static void SetObjectName(uint64 Object, VkObjectType ObjectType, const char* Name);
#endif

static struct ResourceManager* s_ResourceManager = nullptr;
static VulkanContext           s_Context = {};

bool VulkanRendererBackend::ReadObjectPickingBuffer(uint32** OutBuffer, uint32* BufferSize)
{
    *OutBuffer = (uint32*)s_Context.GlobalPickingDataBufferMemory;
    *BufferSize = KRAFT_RENDERER__OBJECT_PICKING_DEPTH_ARRAY_SCALE;

    return true;
}

VulkanContext* VulkanRendererBackend::Context()
{
    return &s_Context;
}

static bool createBuffers();
static void createCommandBuffers();
static void destroyCommandBuffers();
static void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* renderPass);
static void destroyFramebuffers(VulkanSwapchain* swapchain);
static bool instanceLayerSupported(const char* layer, VkLayerProperties* layerProperties, uint32 count);

struct VulkanRendererBackendStateT
{
    VkCommandBuffer BuffersToSubmit[16];
    uint32          BuffersToSubmitNum = 0;
} VulkanRendererBackendState;

bool VulkanRendererBackend::Init(EngineConfigT* config)
{
    KRAFT_VK_CHECK(volkInitialize());
    s_Context = VulkanContext{};

    // NOTE: Right now the renderer settings struct is very simple so we just memcpy it
    memcpy(&s_Context.RendererSettings, &config->RendererSettings, sizeof(VulkanRendererSettings));

    s_ResourceManager = ResourceManager;

    s_Context.AllocationCallbacks = nullptr;
    s_Context.FramebufferWidth = config->WindowWidth;
    s_Context.FramebufferHeight = config->WindowHeight;

    for (int i = 0; i < KRAFT_VULKAN_MAX_GEOMETRIES; i++)
    {
        s_Context.Geometries[i].ID = KRAFT_INVALID_ID;
    }

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pApplicationName = config->ApplicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = config->ApplicationName;
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);

    VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    // Instance level extensions
    const char** extensions = nullptr;
    arrput(extensions, VK_KHR_SURFACE_EXTENSION_NAME);
    arrput(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    arrput(extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#if defined(KRAFT_PLATFORM_WINDOWS)
    arrput(extensions, "VK_KHR_win32_surface");
#elif defined(KRAFT_PLATFORM_MACOS)
    arrput(extensions, "VK_EXT_metal_surface");
#elif defined(KRAFT_PLATFORM_LINUX)
    arrput(extensions, "VK_KHR_xcb_surface");
#endif

#ifdef KRAFT_RENDERER_DEBUG
    arrput(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    KINFO("[VulkanRendererBackend::Init]: Loading the following debug extensions:");
    for (int i = 0; i < arrlen(extensions); ++i)
    {
        KINFO("[VulkanRendererBackend::Init]: %s", extensions[i]);
    }

    instanceCreateInfo.enabledExtensionCount = (uint32)arrlen(extensions);
    instanceCreateInfo.ppEnabledExtensionNames = extensions;

    // Layers
    const char** layers = nullptr;

#ifdef KRAFT_RENDERER_DEBUG
    // Vulkan validation layer
    {
        arrput(layers, "VK_LAYER_KHRONOS_validation");
    }

    // Make sure all the required layers are available
    uint32 availableLayers;
    KRAFT_VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayers, 0));

    VkLayerProperties* availableLayerProperties = nullptr;
    arrsetlen(availableLayerProperties, availableLayers);
    KRAFT_VK_CHECK(vkEnumerateInstanceLayerProperties(&availableLayers, availableLayerProperties));

    for (int i = 0; i < arrlen(layers); ++i)
    {
        bool found = instanceLayerSupported(layers[i], availableLayerProperties, availableLayers);
        if (!found)
        {
            KERROR("[VulkanRendererBackend::Init]: %s layer not found", layers[i]);
            return false;
        }
    }
#endif

    instanceCreateInfo.enabledLayerCount = (uint32)arrlen(layers);
    instanceCreateInfo.ppEnabledLayerNames = layers;

    KRAFT_VK_CHECK(vkCreateInstance(&instanceCreateInfo, s_Context.AllocationCallbacks, &s_Context.Instance));
    arrfree(extensions);
    arrfree(layers);

    volkLoadInstance(s_Context.Instance);

#ifdef KRAFT_RENDERER_DEBUG
    arrfree(availableLayerProperties);
    {
        // Setup vulkan debug callback messenger
        uint32 logLevel = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        createInfo.messageSeverity = logLevel;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugUtilsMessenger;
        createInfo.pUserData = 0;

        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_Context.Instance, "vkCreateDebugUtilsMessengerEXT");
        KRAFT_ASSERT_MESSAGE(func, "Failed to get function address for vkCreateDebugUtilsMessengerEXT");

        KRAFT_VK_CHECK(func(s_Context.Instance, &createInfo, s_Context.AllocationCallbacks, &s_Context.DebugMessenger));
    }

    KRAFT_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(s_Context.Instance, "vkSetDebugUtilsObjectNameEXT");
    s_Context.SetObjectName = SetObjectName;
#endif

    // TODO: These requirements should come from a config or something
    VulkanPhysicalDeviceRequirements requirements = {};
    requirements.Compute = true;
    requirements.Graphics = true;
    requirements.Present = true;
    requirements.Transfer = true;
    requirements.DepthBuffer = true;
    requirements.DeviceExtensionNames = nullptr;
    arrpush(requirements.DeviceExtensionNames, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    arrpush(requirements.DeviceExtensionNames, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

    GLFWwindow* window = Platform::GetWindow()->PlatformWindowHandle;
    KRAFT_VK_CHECK(glfwCreateWindowSurface(s_Context.Instance, window, s_Context.AllocationCallbacks, &s_Context.Surface));

    KSUCCESS("[VulkanRendererBackend::Init]: Successfully created VkSurface");

    VulkanSelectPhysicalDevice(&s_Context, requirements);
    VulkanCreateLogicalDevice(&s_Context, requirements);

    s_Context.GraphicsCommandPool = s_ResourceManager->CreateCommandPool({
        .DebugName = "PrimaryGfxCmdPool",
        .QueueFamilyIndex = s_Context.PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex,
        .Flags = COMMAND_POOL_CREATE_FLAGS_RESET_COMMAND_BUFFER_BIT,
    });

    KDEBUG("[VulkanRendererBackend::Init]: Graphics command pool created");

    arrfree(requirements.DeviceExtensionNames);

    PhysicalDeviceFormatSpecs FormatSpecs;
    {
        switch (s_Context.PhysicalDevice.DepthBufferFormat)
        {
            case VK_FORMAT_D16_UNORM:          FormatSpecs.DepthBufferFormat = Format::D16_UNORM; break;

            case VK_FORMAT_D32_SFLOAT:         FormatSpecs.DepthBufferFormat = Format::D32_SFLOAT; break;

            case VK_FORMAT_D16_UNORM_S8_UINT:  FormatSpecs.DepthBufferFormat = Format::D16_UNORM_S8_UINT; break;

            case VK_FORMAT_D24_UNORM_S8_UINT:  FormatSpecs.DepthBufferFormat = Format::D24_UNORM_S8_UINT; break;

            case VK_FORMAT_D32_SFLOAT_S8_UINT: FormatSpecs.DepthBufferFormat = Format::D32_SFLOAT_S8_UINT; break;

            default:                           KFATAL("Unsupported depth buffer format %d", s_Context.PhysicalDevice.DepthBufferFormat);
        }
    }

    //
    // Create all rendering resources
    //

    VulkanCreateSwapchain(&s_Context, s_Context.FramebufferWidth, s_Context.FramebufferHeight, s_Context.RendererSettings.VSync);
    VulkanCreateRenderPass(
        &s_Context, { 0.10f, 0.10f, 0.10f, 1.0f }, { 0.0f, 0.0f, (float)s_Context.FramebufferWidth, (float)s_Context.FramebufferHeight }, 1.0f, 0, &s_Context.MainRenderPass, true, "MainRenderPass"
    );

    {
        switch (s_Context.Swapchain.ImageFormat.format)
        {
            case VK_FORMAT_R8G8B8A8_UNORM: FormatSpecs.SwapchainFormat = Format::RGBA8_UNORM; break;

            case VK_FORMAT_R8G8B8_UNORM:   FormatSpecs.SwapchainFormat = Format::RGB8_UNORM; break;

            case VK_FORMAT_B8G8R8A8_UNORM: FormatSpecs.SwapchainFormat = Format::BGRA8_UNORM; break;

            case VK_FORMAT_B8G8R8_UNORM:   FormatSpecs.SwapchainFormat = Format::BGR8_UNORM; break;

            default:                       KFATAL("Unsupported swapchain image format %d", s_Context.Swapchain.ImageFormat.format);
        }

        s_ResourceManager->SetPhysicalDeviceFormatSpecs(FormatSpecs);
    }

    createFramebuffers(&s_Context.Swapchain, &s_Context.MainRenderPass);
    createCommandBuffers();

    CreateArray(s_Context.ImageAvailableSemaphores, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.RenderCompleteSemaphores, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.InFlightImageToFenceMap, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.WaitFences, s_Context.Swapchain.ImageCount);

    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        VkSemaphoreCreateInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &info, s_Context.AllocationCallbacks, &s_Context.ImageAvailableSemaphores[i]);
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &info, s_Context.AllocationCallbacks, &s_Context.RenderCompleteSemaphores[i]);

        VulkanCreateFence(&s_Context, true, &s_Context.WaitFences[i]);
        s_Context.InFlightImageToFenceMap[i] = &s_Context.WaitFences[i];
    }

    // Global Descriptor Sets
    {
        // First, we create our descriptor pool
        const uint32         GlobalPoolSize = 128 * s_Context.Swapchain.ImageCount;
        VkDescriptorPoolSize PoolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, GlobalPoolSize },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, GlobalPoolSize },
        };

        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        DescriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        DescriptorPoolCreateInfo.poolSizeCount = sizeof(PoolSizes) / sizeof(PoolSizes[0]);
        DescriptorPoolCreateInfo.pPoolSizes = &PoolSizes[0];
        DescriptorPoolCreateInfo.maxSets = GlobalPoolSize * sizeof(PoolSizes) / sizeof(PoolSizes[0]);

        vkCreateDescriptorPool(s_Context.LogicalDevice.Handle, &DescriptorPoolCreateInfo, s_Context.AllocationCallbacks, &s_Context.GlobalDescriptorPool);

        // Descriptor sets
        VkDescriptorSetLayoutBinding GlobalDataLayoutBinding[2] = {};
        GlobalDataLayoutBinding[0].binding = 0;
        GlobalDataLayoutBinding[0].descriptorCount = 1;
        GlobalDataLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        GlobalDataLayoutBinding[0].pImmutableSamplers = 0;
        GlobalDataLayoutBinding[0].stageFlags = VK_SHADER_STAGE_ALL;

        GlobalDataLayoutBinding[1].binding = 1;
        GlobalDataLayoutBinding[1].descriptorCount = 1;
        GlobalDataLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        GlobalDataLayoutBinding[1].pImmutableSamplers = 0;
        GlobalDataLayoutBinding[1].stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo GlobalDataLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        GlobalDataLayoutCreateInfo.bindingCount = KRAFT_C_ARRAY_SIZE(GlobalDataLayoutBinding);
        GlobalDataLayoutCreateInfo.pBindings = &GlobalDataLayoutBinding[0];
        GlobalDataLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &GlobalDataLayoutCreateInfo, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        VkDescriptorSetLayoutBinding MaterialDataLayoutBinding = {};
        MaterialDataLayoutBinding.binding = 0;
        MaterialDataLayoutBinding.descriptorCount = 1;
        MaterialDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        MaterialDataLayoutBinding.pImmutableSamplers = 0;
        MaterialDataLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo MaterialDataLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        MaterialDataLayoutCreateInfo.bindingCount = 1;
        MaterialDataLayoutCreateInfo.pBindings = &MaterialDataLayoutBinding;
        GlobalDataLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &MaterialDataLayoutCreateInfo, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        VkDescriptorSetLayout Layouts[16 * KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
        for (int i = 0; i < s_Context.DescriptorSetLayoutsCount; i++)
        {
            for (int j = 0; j < s_Context.Swapchain.ImageCount; j++)
            {
                Layouts[i * s_Context.Swapchain.ImageCount + j] = s_Context.DescriptorSetLayouts[i];
            }
        }

        // VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        // DescriptorSetAllocateInfo.descriptorPool = s_Context.GlobalDescriptorPool;
        // DescriptorSetAllocateInfo.descriptorSetCount = s_Context.DescriptorSetLayoutsCount * s_Context.Swapchain.ImageCount;
        // DescriptorSetAllocateInfo.pSetLayouts = &Layouts[0];

        // KRAFT_VK_CHECK(vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &DescriptorSetAllocateInfo, &s_Context.GlobalDescriptorSets[0][0]));

        // Create the uniform buffer for data upload
        uint64 ExtraMemoryFlags = s_Context.PhysicalDevice.SupportsDeviceLocalHostVisible ? MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL : 0;
        s_Context.GlobalUniformBuffer = ResourceManager->CreateBuffer({
            .DebugName = "GlobalUniformBuffer",
            .Size = (uint64)math::AlignUp(sizeof(GlobalShaderData), s_Context.PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment),
            .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
            .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | ExtraMemoryFlags,
            .MapMemory = true,
        });

        s_Context.GlobalPickingDataBuffer = ResourceManager->CreateBuffer({
            .DebugName = "GlobalPickingDataBuffer",
            .Size = (uint64)math::AlignUp(KRAFT_RENDERER__OBJECT_PICKING_DEPTH_ARRAY_SCALE * sizeof(uint32), s_Context.PhysicalDevice.Properties.limits.minStorageBufferOffsetAlignment),
            .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER,
            .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | ExtraMemoryFlags,
            .MapMemory = true,
        });

        // Map the buffer memory
        s_Context.GlobalUniformBufferMemory = ResourceManager->GetBufferData(s_Context.GlobalUniformBuffer);
        s_Context.GlobalPickingDataBufferMemory = ResourceManager->GetBufferData(s_Context.GlobalPickingDataBuffer);

        // for (int i = 0; i < s_Context.DescriptorSetLayoutsCount; i++)
        // {
        //     for (int j = 0; j < s_Context.Swapchain.ImageCount; j++)
        //     {
        //         KString ObjName = "GlobalDescriptorSet";
        //         ObjName.Append(i + '0');
        //         ObjName.Append(j + '0');
        //         SetObjectName((uint64)s_Context.GlobalDescriptorSets[i][j], VK_OBJECT_TYPE_DESCRIPTOR_SET, *ObjName);
        //     }
        // }
    }

    // Create vertex and index buffers
    if (!createBuffers())
    {
        KFATAL("[VulkanRendererBackend::Init]: Failed to create buffers");
        return false;
    }

    KSUCCESS("[VulkanRendererBackend::Init]: Backend init success!");

    return true;
}

bool VulkanRendererBackend::Shutdown()
{
    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);
    s_ResourceManager->Clear();

    vkDestroyDescriptorPool(s_Context.LogicalDevice.Handle, s_Context.GlobalDescriptorPool, s_Context.AllocationCallbacks);
    // vkDestroyDescriptorSetLayout(s_Context.LogicalDevice.Handle, GlobalDescriptorSetLayout, s_Context.AllocationCallbacks);

    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        vkDestroySemaphore(s_Context.LogicalDevice.Handle, s_Context.ImageAvailableSemaphores[i], s_Context.AllocationCallbacks);
        s_Context.ImageAvailableSemaphores[i] = 0;

        vkDestroySemaphore(s_Context.LogicalDevice.Handle, s_Context.RenderCompleteSemaphores[i], s_Context.AllocationCallbacks);
        s_Context.RenderCompleteSemaphores[i] = 0;

        VulkanDestroyFence(&s_Context, &s_Context.WaitFences[i]);
    }

    DestroyArray(s_Context.ImageAvailableSemaphores);
    DestroyArray(s_Context.RenderCompleteSemaphores);
    DestroyArray(s_Context.WaitFences);

    // Destroy command buffers
    // Destroyed when the resource manager is destroyed
    // destroyCommandBuffers();

    // Destroy framebuffers
    destroyFramebuffers(&s_Context.Swapchain);

    VulkanDestroyRenderPass(&s_Context, &s_Context.MainRenderPass);
    VulkanDestroySwapchain(&s_Context);
    VulkanDestroyLogicalDevice(&s_Context);

    if (s_Context.Surface)
    {
        vkDestroySurfaceKHR(s_Context.Instance, s_Context.Surface, s_Context.AllocationCallbacks);
        s_Context.Surface = 0;
    }

    // s_ResourceManager->Destroy();

    return true;
}

int VulkanRendererBackend::PrepareFrame()
{
    VulkanFence* fence = s_Context.InFlightImageToFenceMap[s_Context.Swapchain.CurrentFrame];
    if (!VulkanWaitForFence(&s_Context, fence, UINT64_MAX))
    {
        KERROR("[VulkanRendererBackend::PrepareFrame]: VulkanWaitForFence failed");
        return -1;
    }

    VulkanResetFence(&s_Context, fence);

    // Acquire the next image
    if (!VulkanAcquireNextImageIndex(&s_Context, UINT64_MAX, s_Context.ImageAvailableSemaphores[s_Context.Swapchain.CurrentFrame], 0, &s_Context.CurrentSwapchainImageIndex))
    {
        return -1;
    }

    // Record commands
    // s_Context.ActiveCommandBuffer = s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];

    // VulkanCommandBuffer* GPUCmdBuffer = s_ResourceManager->GetCommandBufferPool().Get(s_Context.ActiveCommandBuffer);
    // VulkanResetCommandBuffer(GPUCmdBuffer);
    // VulkanBeginCommandBuffer(GPUCmdBuffer, false, false, false);

    // return s_Context.Swapchain.CurrentFrame;
    return s_Context.CurrentSwapchainImageIndex;
}

bool VulkanRendererBackend::BeginFrame()
{
    // Record commands
    s_Context.ActiveCommandBuffer = s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];

    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, s_Context.ActiveCommandBuffer);
    VulkanResetCommandBuffer(GPUCmdBuffer);
    VulkanBeginCommandBuffer(GPUCmdBuffer, false, false, false);

    // VulkanCommandBuffer* GPUCmdBuffer = s_ResourceManager->GetCommandBufferPool().Get(s_Context.ActiveCommandBuffer);
    VulkanBeginRenderPass(GPUCmdBuffer, &s_Context.MainRenderPass, s_Context.Swapchain.Framebuffers[s_Context.CurrentSwapchainImageIndex].Handle, VK_SUBPASS_CONTENTS_INLINE);

    VulkanRendererBackendState.BuffersToSubmit[VulkanRendererBackendState.BuffersToSubmitNum] = GPUCmdBuffer->Resource;
    VulkanRendererBackendState.BuffersToSubmitNum++;

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = (float32)s_Context.FramebufferHeight;
    viewport.width = (float32)s_Context.FramebufferWidth;
    viewport.height = -(float32)s_Context.FramebufferHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(GPUCmdBuffer->Resource, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = { s_Context.FramebufferWidth, s_Context.FramebufferHeight };

    vkCmdSetScissor(GPUCmdBuffer->Resource, 0, 1, &scissor);

    s_Context.MainRenderPass.Rect.z = (float32)s_Context.FramebufferWidth;
    s_Context.MainRenderPass.Rect.w = (float32)s_Context.FramebufferHeight;

    return true;
}

bool VulkanRendererBackend::EndFrame()
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, s_Context.ActiveCommandBuffer);
    VulkanEndRenderPass(GPUCmdBuffer, &s_Context.MainRenderPass);

    VulkanEndCommandBuffer(GPUCmdBuffer);

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo         info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = VulkanRendererBackendState.BuffersToSubmitNum;
    info.pCommandBuffers = &VulkanRendererBackendState.BuffersToSubmit[0];
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &s_Context.ImageAvailableSemaphores[s_Context.Swapchain.CurrentFrame];
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &s_Context.RenderCompleteSemaphores[s_Context.Swapchain.CurrentFrame];
    info.pWaitDstStageMask = &waitStageMask;

    KRAFT_VK_CHECK(vkQueueSubmit(s_Context.LogicalDevice.GraphicsQueue, 1, &info, s_Context.InFlightImageToFenceMap[s_Context.Swapchain.CurrentFrame]->Handle));

    VulkanSetCommandBufferSubmitted(GPUCmdBuffer);

    VulkanPresentSwapchain(&s_Context, s_Context.LogicalDevice.PresentQueue, s_Context.RenderCompleteSemaphores[s_Context.Swapchain.CurrentFrame], s_Context.CurrentSwapchainImageIndex);

    VulkanRendererBackendState.BuffersToSubmitNum = 0;

    // uint32* BufferData = (uint32*)s_Context.GlobalPickingDataBufferMemory;
    // for (size_t i = 0; i < KRAFT_RENDERER__OBJECT_PICKING_DEPTH_ARRAY_SCALE; i++)
    // {
    //     if (BufferData[i] != 0)
    //     {
    //         uint32 SelectedEntity = BufferData[i];
    //         KDEBUG("Selected: %d", SelectedEntity);
    //         break;
    //     }
    // }

    MemSet(s_Context.GlobalPickingDataBufferMemory, 0, sizeof(uint32) * KRAFT_RENDERER__OBJECT_PICKING_DEPTH_ARRAY_SCALE);

    return true;
}

void VulkanRendererBackend::OnResize(int width, int height)
{
    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);

    if (width == 0 || height == 0)
    {
        KERROR("Cannot recreate swapchain because width/height is 0 | %d, %d", width, height);
        return;
    }

    s_Context.FramebufferWidth = width;
    s_Context.FramebufferHeight = height;

    VulkanRecreateSwapchain(&s_Context);

    s_Context.MainRenderPass.Rect = { 0.0f, 0.0f, (float)s_Context.FramebufferWidth, (float)s_Context.FramebufferHeight };

    destroyCommandBuffers();
    createCommandBuffers();
    createFramebuffers(&s_Context.Swapchain, &s_Context.MainRenderPass);
}

void VulkanRendererBackend::CreateRenderPipeline(Shader* Shader, int PassIndex, Handle<RenderPass> RenderPassHandle)
{
    const ShaderEffect& Effect = Shader->ShaderEffect;
    uint64              RenderPassesCount = Effect.RenderPasses.Length;

    const RenderPassDefinition&  Pass = Effect.RenderPasses[PassIndex];
    VkGraphicsPipelineCreateInfo PipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    // Shader stages
    uint64                ShaderStagesCount = Pass.ShaderStages.Length;
    auto                  PipelineShaderStageCreateInfos = Array<VkPipelineShaderStageCreateInfo>(ShaderStagesCount);
    Array<VkShaderModule> ShaderModules(ShaderStagesCount);
    for (int j = 0; j < ShaderStagesCount; j++)
    {
        const RenderPassDefinition::ShaderDefinition& Shader = Pass.ShaderStages[j];
        VkShaderModule                                ShaderModule;
        VulkanCreateShaderModule(&s_Context, Shader.CodeFragment.Code.Data(), Shader.CodeFragment.Code.Length, &ShaderModule);
        KASSERT(ShaderModule);

        VkPipelineShaderStageCreateInfo ShaderStageInfo = {};
        ShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStageInfo.module = ShaderModule;
        ShaderStageInfo.pName = "main";
        ShaderStageInfo.stage = ToVulkanShaderStageFlagBits(Shader.Stage);

        PipelineShaderStageCreateInfos[j] = ShaderStageInfo;
        ShaderModules[j] = ShaderModule;
    }

    // Input bindings and attributes
    uint64 InputBindingsCount = Pass.VertexLayout->InputBindings.Length;
    auto   InputBindingsDesc = Array<VkVertexInputBindingDescription>(InputBindingsCount);
    for (int j = 0; j < InputBindingsCount; j++)
    {
        const VertexInputBinding& InputBinding = Pass.VertexLayout->InputBindings[j];
        InputBindingsDesc[j].binding = InputBinding.Binding;
        InputBindingsDesc[j].inputRate = InputBinding.InputRate == VertexInputRate::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        InputBindingsDesc[j].stride = InputBinding.Stride;
    }

    uint64 AttributesCount = Pass.VertexLayout->Attributes.Length;
    auto   AttributesDesc = Array<VkVertexInputAttributeDescription>(AttributesCount);
    for (int j = 0; j < AttributesCount; j++)
    {
        const VertexAttribute& VertexAttribute = Pass.VertexLayout->Attributes[j];
        AttributesDesc[j].location = VertexAttribute.Location;
        AttributesDesc[j].format = ToVulkanFormat(VertexAttribute.Format);
        AttributesDesc[j].binding = VertexAttribute.Binding;
        AttributesDesc[j].offset = VertexAttribute.Offset;
    }

    // Shader resources
    uint64 ResourceBindingsCount = Pass.Resources != nullptr ? Pass.Resources->ResourceBindings.Length : 0;
    uint64 ConstantBuffersCount = Pass.ConstantBuffers->Fields.Length;

    uint64 InstanceUBOSize = 128; // TODO: DONT HARDCODE
    auto   DescriptorSetLayoutBindings = Array<VkDescriptorSetLayoutBinding>(ResourceBindingsCount);
    for (int j = 0; j < ResourceBindingsCount; j++)
    {
        const ResourceBinding& Binding = Pass.Resources->ResourceBindings[j];
        DescriptorSetLayoutBindings[j].binding = Binding.Binding;

        // TODO (amn): This descriptor count must be dynamic, based on the shader
        // because shaders could have array resources and in that case this should be
        // equal to the array size in the shader
        // Docs: https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html
        DescriptorSetLayoutBindings[j].descriptorCount = 1;
        DescriptorSetLayoutBindings[j].descriptorType = ToVulkanResourceType(Binding.Type);
        DescriptorSetLayoutBindings[j].pImmutableSamplers = 0;
        DescriptorSetLayoutBindings[j].stageFlags = ToVulkanShaderStageFlags(Binding.Stage);

        if (Binding.Type == ResourceType::UniformBuffer)
        {
            // InstanceUBOSize += Binding.Size;
        }
    }

    Shader->InstanceUBOStride = (uint32)math::AlignUp(InstanceUBOSize, s_Context.PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment);
    uint64 UBOSize = math::AlignUp(Shader->InstanceUBOStride * KRAFT_MATERIAL_MAX_INSTANCES, s_Context.PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment);

    // Constant Buffers
    uint32 ConstantBufferSize = 0;
    auto   PushConstantRanges = Array<VkPushConstantRange>(ConstantBuffersCount);
    for (int j = 0; j < ConstantBuffersCount; j++)
    {
        const ConstantBufferEntry& Entry = Pass.ConstantBuffers->Fields[j];
        uint32                     AlignedSize = (uint32)math::AlignUp(ShaderDataType::SizeOf(Entry.Type), 4);
        PushConstantRanges[j].size = 128;
        PushConstantRanges[j].offset = ConstantBufferSize;
        PushConstantRanges[j].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        ConstantBufferSize += AlignedSize;
    }

    KASSERTM(ConstantBufferSize <= 128, "ConstantBuffers cannot be larger than 128 bytes");

    VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};
    ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportStateCreateInfo.viewportCount = 1;
    ViewportStateCreateInfo.scissorCount = 1;
    PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;

    VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo = {};
    RasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    RasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    RasterizationStateCreateInfo.polygonMode = ToVulkanPolygonMode(Pass.RenderState->PolygonMode);
    RasterizationStateCreateInfo.lineWidth = s_Context.PhysicalDevice.Features.wideLines ? Pass.RenderState->LineWidth : 1.0f;
    RasterizationStateCreateInfo.cullMode = ToVulkanCullModeFlagBits(Pass.RenderState->CullMode);
    // If culling is eanbled & counter-clockwise is specified, the vertices
    // for any triangle must be specified in a counter-clockwise fashion.
    // Vice-versa for clockwise.
    RasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    RasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    RasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    RasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    PipelineCreateInfo.pRasterizationState = &RasterizationStateCreateInfo;

    VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {};
    MultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    MultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    MultisampleStateCreateInfo.minSampleShading = 1.0f;
    MultisampleStateCreateInfo.pSampleMask = 0;
    MultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    MultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;
    PipelineCreateInfo.pMultisampleState = &MultisampleStateCreateInfo;

    VkPipelineDepthStencilStateCreateInfo DepthStencilStateCreateInfo = {};
    DepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencilStateCreateInfo.depthTestEnable = Pass.RenderState->ZTestOperation > CompareOp::Never ? VK_TRUE : VK_FALSE;
    DepthStencilStateCreateInfo.depthWriteEnable = Pass.RenderState->ZWriteEnable;
    DepthStencilStateCreateInfo.depthCompareOp = ToVulkanCompareOp(Pass.RenderState->ZTestOperation);
    DepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    DepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    PipelineCreateInfo.pDepthStencilState = &DepthStencilStateCreateInfo;

    VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {};
    ColorBlendAttachmentState.blendEnable = Pass.RenderState->BlendEnable;
    if (Pass.RenderState->BlendEnable)
    {
        ColorBlendAttachmentState.srcColorBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.SrcColorBlendFactor);
        ColorBlendAttachmentState.dstColorBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.DstColorBlendFactor);
        ColorBlendAttachmentState.colorBlendOp = ToVulkanBlendOp(Pass.RenderState->BlendMode.ColorBlendOperation);
        ColorBlendAttachmentState.srcAlphaBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.SrcAlphaBlendFactor);
        ColorBlendAttachmentState.dstAlphaBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.DstAlphaBlendFactor);
        ColorBlendAttachmentState.alphaBlendOp = ToVulkanBlendOp(Pass.RenderState->BlendMode.AlphaBlendOperation);
    }

    ColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};
    ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendStateCreateInfo.attachmentCount = 1;
    ColorBlendStateCreateInfo.pAttachments = &ColorBlendAttachmentState;
    ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;

    VkDynamicState DynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo = {};
    DynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    DynamicStateCreateInfo.pDynamicStates = DynamicStates;
    DynamicStateCreateInfo.dynamicStateCount = sizeof(DynamicStates) / sizeof(DynamicStates[0]);
    PipelineCreateInfo.pDynamicState = &DynamicStateCreateInfo;

    VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {};
    VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputStateCreateInfo.pVertexBindingDescriptions = &InputBindingsDesc[0];
    VertexInputStateCreateInfo.vertexBindingDescriptionCount = (uint32)InputBindingsDesc.Length;
    VertexInputStateCreateInfo.pVertexAttributeDescriptions = &AttributesDesc[0];
    VertexInputStateCreateInfo.vertexAttributeDescriptionCount = (uint32)AttributesDesc.Length;
    PipelineCreateInfo.pVertexInputState = &VertexInputStateCreateInfo;

    VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo = {};
    InputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;
    InputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineCreateInfo.pInputAssemblyState = &InputAssemblyStateCreateInfo;

    VkDescriptorSetLayout DescriptorSetLayouts[16] = { 0 };
    int                   LayoutsCount = 0;
    while (LayoutsCount < s_Context.DescriptorSetLayoutsCount)
    {
        DescriptorSetLayouts[LayoutsCount++] = s_Context.DescriptorSetLayouts[LayoutsCount];
    }

    VulkanShader* VulkanShaderData = (VulkanShader*)Malloc(sizeof(VulkanShader), MEMORY_TAG_RENDERER, true);

    // Do we have any shader specific local resources?
    if (ResourceBindingsCount > 0)
    {
        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        DescriptorSetLayoutCreateInfo.bindingCount = (uint32)DescriptorSetLayoutBindings.Length;
        DescriptorSetLayoutCreateInfo.pBindings = &DescriptorSetLayoutBindings[0];

        VkDescriptorSetLayout LocalDescriptorSetLayout;
        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(s_Context.LogicalDevice.Handle, &DescriptorSetLayoutCreateInfo, s_Context.AllocationCallbacks, &LocalDescriptorSetLayout));

        DescriptorSetLayouts[LayoutsCount++] = LocalDescriptorSetLayout;
        VulkanShaderData->ShaderResources.LocalDescriptorSetLayout = LocalDescriptorSetLayout;
    }

    VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.setLayoutCount = LayoutsCount;
    LayoutCreateInfo.pSetLayouts = &DescriptorSetLayouts[0];
    LayoutCreateInfo.pushConstantRangeCount = 1;
    // LayoutCreateInfo.pushConstantRangeCount = (uint32)PushConstantRanges.Length;
    // LayoutCreateInfo.pPushConstantRanges = &PushConstantRanges[0];
    auto PCRange = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 128,
    };
    LayoutCreateInfo.pPushConstantRanges = &PCRange;

    VkPipelineLayout PipelineLayout;
    KRAFT_VK_CHECK(vkCreatePipelineLayout(s_Context.LogicalDevice.Handle, &LayoutCreateInfo, s_Context.AllocationCallbacks, &PipelineLayout));
    KASSERT(PipelineLayout);
    PipelineCreateInfo.layout = PipelineLayout;

    PipelineCreateInfo.stageCount = (uint32)PipelineShaderStageCreateInfos.Length;
    PipelineCreateInfo.pStages = &PipelineShaderStageCreateInfos[0];
    PipelineCreateInfo.pTessellationState = 0;

    // PipelineCreateInfo.renderPass = s_Context.MainRenderPass.Handle;
    PipelineCreateInfo.renderPass = RenderPassHandle ? VulkanResourceManagerApi::GetRenderPass(s_ResourceManager->State, RenderPassHandle)->Handle : s_Context.MainRenderPass.Handle;
    PipelineCreateInfo.subpass = 0;
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;

    VkPipeline Pipeline;
    KRAFT_VK_CHECK(vkCreateGraphicsPipelines(s_Context.LogicalDevice.Handle, VK_NULL_HANDLE, 1, &PipelineCreateInfo, s_Context.AllocationCallbacks, &Pipeline));

    KASSERT(Pipeline);

    // Create the uniform buffer for data upload
    uint64 ExtraMemoryFlags = s_Context.PhysicalDevice.SupportsDeviceLocalHostVisible ? MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL : 0;
    VulkanShaderData->ShaderResources.UniformBuffer = ResourceManager->CreateBuffer({
        .DebugName = "UniformBuffer",
        .Size = UBOSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | ExtraMemoryFlags,
        .MapMemory = true,
    });

    // Map the buffer memory
    VulkanShaderData->ShaderResources.UniformBufferMemory = ResourceManager->GetBufferData(VulkanShaderData->ShaderResources.UniformBuffer);
    VulkanShaderData->Pipeline = Pipeline;
    VulkanShaderData->PipelineLayout = PipelineLayout;
    Shader->RendererData = VulkanShaderData;

    // Release shader modules
    for (int j = 0; j < ShaderModules.Length; j++)
    {
        VulkanDestroyShaderModule(&s_Context, &ShaderModules[j]);
    }
}

void VulkanRendererBackend::DestroyRenderPipeline(Shader* Shader)
{
    if (Shader->RendererData)
    {
        VulkanShader*         VulkanShaderData = (VulkanShader*)Shader->RendererData;
        VulkanShaderResources ShaderResources = VulkanShaderData->ShaderResources;
        VkPipeline            Pipeline = VulkanShaderData->Pipeline;
        VkPipelineLayout      PipelineLayout = VulkanShaderData->PipelineLayout;

        ResourceManager->DestroyBuffer(ShaderResources.UniformBuffer);

        // VulkanDestroyBuffer(&s_Context, &ShaderResources.UniformBuffer);

        vkDestroyDescriptorSetLayout(s_Context.LogicalDevice.Handle, ShaderResources.LocalDescriptorSetLayout, s_Context.AllocationCallbacks);

        // vkFreeDescriptorSets(s_Context.LogicalDevice.Handle, GlobalDescriptorPool, ShaderResources->DescriptorSets.Length, ShaderResources->DescriptorSets.Data());

        vkDestroyPipeline(s_Context.LogicalDevice.Handle, Pipeline, s_Context.AllocationCallbacks);
        vkDestroyPipelineLayout(s_Context.LogicalDevice.Handle, PipelineLayout, s_Context.AllocationCallbacks);

        Free(Shader->RendererData, sizeof(VulkanShader), MEMORY_TAG_RENDERER);
        Shader->RendererData = 0;
    }
}

void VulkanRendererBackend::CreateMaterial(Material* Material)
{
    Shader*                     Shader = Material->Shader;
    const ShaderEffect&         Effect = Shader->ShaderEffect;
    const RenderPassDefinition& Pass = Effect.RenderPasses[0]; // TODO: What to do with pass index?

    // Material Data is only required when a shader has any local resources

    VulkanShader* VulkanShaderData = (VulkanShader*)Shader->RendererData;

    // Look for a free slot in the shader UBO
    int FreeUBOSlot = -1;
    for (int i = 0; i < 128; i++)
    {
        if (!VulkanShaderData->ShaderResources.UsedSlots[i].Used)
        {
            FreeUBOSlot = i;
            VulkanShaderData->ShaderResources.UsedSlots[i].Used = true;
            break;
        }
    }

    if (FreeUBOSlot == -1)
    {
        KERROR("[VulkanRendererBackend::CreateMaterial]: There are no free slots in the shader UBO");
        return;
    }

    VulkanMaterialData* MaterialRendererData = &s_Context.Materials[Material->ID];
    MaterialRendererData->InstanceUBOOffset = (Shader->InstanceUBOStride * FreeUBOSlot);
    VkDescriptorSetLayout DescriptorSetLayout = VulkanShaderData->ShaderResources.LocalDescriptorSetLayout;
    VkDescriptorSetLayout LocalLayouts[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES] = { DescriptorSetLayout, DescriptorSetLayout, DescriptorSetLayout };

    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    DescriptorSetAllocateInfo.descriptorPool = s_Context.GlobalDescriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount = s_Context.Swapchain.ImageCount;
    DescriptorSetAllocateInfo.pSetLayouts = &LocalLayouts[0];

    VkResult AllocationResult = vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &DescriptorSetAllocateInfo, &MaterialRendererData->LocalDescriptorSets[0]);
    if (AllocationResult == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        // TODO (amn): Solve this
        KFATAL("Pool ran out of memory");
    }

    // Invalidate all the descriptor binding states so we write to them correctly on the first draw call
    for (int BindingIndex = 0; BindingIndex < Pass.Resources->ResourceBindings.Length; BindingIndex++)
    {
        for (int j = 0; j < KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES; j++)
        {
            MaterialRendererData->DescriptorStates[BindingIndex].Generations[j] = KRAFT_INVALID_ID_UINT8;
        }
    }

    Material->RendererDataIdx = Material->ID;
}

void VulkanRendererBackend::DestroyMaterial(Material* Material)
{
    // Shader* Shader = Material->Shader;

    // VulkanMaterialData* MaterialRendererData = (VulkanMaterialData*)Material->RendererData;
    // // vkFreeDescriptorSets(s_Context.LogicalDevice.Handle, GlobalDescriptorPool, MaterialInstance->DescriptorSets.Length, MaterialInstance->DescriptorSets.Data());

    // Free(MaterialRendererData, sizeof(VulkanMaterialData), MEMORY_TAG_RENDERER);
    // Material->RendererData = 0;
}

void VulkanRendererBackend::SetGlobalShaderData(GlobalShaderData* Data)
{
    MemCpy(s_Context.GlobalUniformBufferMemory, Data, sizeof(GlobalShaderData));
}

void VulkanRendererBackend::UseShader(const Shader* Shader)
{
    VulkanShader*        VulkanShaderData = (VulkanShader*)Shader->RendererData;
    VkPipeline           Pipeline = VulkanShaderData->Pipeline;
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, s_Context.ActiveCommandBuffer);
    ;
    vkCmdBindPipeline(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
}

void VulkanRendererBackend::SetUniform(Shader* Shader, const ShaderUniform& Uniform, void* Value, bool Invalidate)
{
    VulkanShader*        VulkanShaderData = (VulkanShader*)Shader->RendererData;
    VkPipeline           Pipeline = VulkanShaderData->Pipeline;
    VkPipelineLayout     PipelineLayout = VulkanShaderData->PipelineLayout;
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, s_Context.ActiveCommandBuffer);
    ;

    if (Uniform.Scope == ShaderUniformScope::Local)
    {
        vkCmdPushConstants(GPUCmdBuffer->Resource, PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, Uniform.Offset, Uniform.Stride, Value);
    }
    else if (Uniform.Scope == ShaderUniformScope::Global)
    {
        MemCpy(s_Context.GlobalUniformBufferMemory, Value, sizeof(GlobalShaderData));
    }
    else
    {
        if (Uniform.Scope == ShaderUniformScope::Instance)
        {
            Material*           MaterialInstance = Shader->ActiveMaterial;
            VulkanMaterialData* MaterialData = &s_Context.Materials[MaterialInstance->RendererDataIdx];

            if (Uniform.Type == ResourceType::Sampler)
            {
                MaterialInstance->Textures[Uniform.Offset] = *(Handle<Texture>*)Value;
            }
            else
            {
                void* Offset = (char*)VulkanShaderData->ShaderResources.UniformBufferMemory + MaterialData->InstanceUBOOffset + Uniform.Offset;
                MemCpy(Offset, Value, Uniform.Stride);
            }

            if (Invalidate)
            {
                const int MaxSwapchainImages = 3;
                for (int j = 0; j < MaxSwapchainImages; j++)
                {
                    MaterialData->DescriptorStates[Uniform.Location].Generations[j] = KRAFT_INVALID_ID_UINT8;
                }
            }
        }
        else
        {
            // TODO: Global textures
            void* Offset = (char*)VulkanShaderData->ShaderResources.UniformBufferMemory + Uniform.Offset;
            MemCpy(Offset, Value, Uniform.Stride);
        }
    }
}

void VulkanRendererBackend::ApplyGlobalShaderProperties(Shader* Shader, Handle<Buffer> GlobalUBOBuffer)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, s_Context.ActiveCommandBuffer);
    VulkanShader*        ShaderData = (VulkanShader*)Shader->RendererData;

    VkBuffer               GPUBuffer = VulkanResourceManagerApi::GetBuffer(s_ResourceManager->State, GlobalUBOBuffer.IsInvalid() ? s_Context.GlobalUniformBuffer : GlobalUBOBuffer)->Handle;
    VkDescriptorBufferInfo GlobalDataBufferInfo = {};
    GlobalDataBufferInfo.buffer = GPUBuffer;
    GlobalDataBufferInfo.offset = 0;
    GlobalDataBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet DescriptorWriteInfo[2] = {};
    DescriptorWriteInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWriteInfo[0].descriptorCount = 1;
    DescriptorWriteInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    DescriptorWriteInfo[0].dstBinding = 0;
    DescriptorWriteInfo[0].dstArrayElement = 0;
    DescriptorWriteInfo[0].pBufferInfo = &GlobalDataBufferInfo;
    // DescriptorWriteInfo.dstSet = s_Context.GlobalDescriptorSets[0][s_Context.CurrentSwapchainImageIndex];

    VkBuffer               PickingGPUBuffer = VulkanResourceManagerApi::GetBuffer(s_ResourceManager->State, s_Context.GlobalPickingDataBuffer)->Handle;
    VkDescriptorBufferInfo GlobalPickingDataBufferInfo = {};
    GlobalPickingDataBufferInfo.buffer = PickingGPUBuffer;
    GlobalPickingDataBufferInfo.offset = 0;
    GlobalPickingDataBufferInfo.range = VK_WHOLE_SIZE;

    DescriptorWriteInfo[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWriteInfo[1].descriptorCount = 1;
    DescriptorWriteInfo[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    DescriptorWriteInfo[1].dstBinding = 1;
    DescriptorWriteInfo[1].dstArrayElement = 0;
    DescriptorWriteInfo[1].pBufferInfo = &GlobalPickingDataBufferInfo;

    // vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, 1, &DescriptorWriteInfo, 0, 0);

    // VkDescriptorSet SetsToBind[] = {
    //     s_Context.GlobalDescriptorSets[0][s_Context.CurrentSwapchainImageIndex],
    //     s_Context.GlobalDescriptorSets[1][s_Context.CurrentSwapchainImageIndex],
    // };

    // vkCmdBindDescriptorSets(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, ShaderData->PipelineLayout, 0, KRAFT_C_ARRAY_SIZE(SetsToBind), &SetsToBind[0], 0, 0);

    vkCmdPushDescriptorSetKHR(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, ShaderData->PipelineLayout, 0, KRAFT_C_ARRAY_SIZE(DescriptorWriteInfo), &DescriptorWriteInfo[0]);
}

void VulkanRendererBackend::ApplyInstanceShaderProperties(Shader* Shader)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, s_Context.ActiveCommandBuffer);
    Material*            MaterialInstance = Shader->ActiveMaterial;
    VulkanMaterialData*  MaterialData = &s_Context.Materials[MaterialInstance->RendererDataIdx];
    uint32               WriteCount = 0;
    VkWriteDescriptorSet WriteInfos[32] = {};
    VkWriteDescriptorSet DescriptorWriteInfo = {};

    VulkanShader*          ShaderData = (VulkanShader*)Shader->RendererData;
    VulkanShaderResources* ShaderResources = &ShaderData->ShaderResources;

    VkDescriptorBufferInfo DescriptorBufferInfo = {};
    VkDescriptorImageInfo  DescriptorImageInfo[32] = {};

    // uint64 ResourceBindingsCount = Shader->ShaderEffect.RenderPasses[0].Resources->ResourceBindings.Length;
    // for (int j = 0; j < ResourceBindingsCount; j++)
    // {
    //     const ResourceBinding& Binding = Shader->ShaderEffect.RenderPasses[0].Resources->ResourceBindings[j];
    //     DescriptorSetLayoutBindings[j].binding = Binding.Binding;

    //     // TODO (amn): This descriptor count must be dynamic, based on the shader
    //     // because shaders could have array resources and in that case this should be
    //     // equal to the array size in the shader
    //     // Docs: https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html
    //     DescriptorSetLayoutBindings[j].descriptorCount = 1;
    //     DescriptorSetLayoutBindings[j].descriptorType = ToVulkanResourceType(Binding.Type);
    //     DescriptorSetLayoutBindings[j].pImmutableSamplers = 0;
    //     DescriptorSetLayoutBindings[j].stageFlags = ToVulkanShaderStageFlags(Binding.Stage);

    //     if (Binding.Type == ResourceType::UniformBuffer)
    //     {
    //         // InstanceUBOSize += Binding.Size;
    //     }
    // }

    uint32 BindingIndex = 0;
    // Material data
    if (MaterialData->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex] == KRAFT_INVALID_ID_UINT8)
    {
        DescriptorBufferInfo.buffer = VulkanResourceManagerApi::GetBuffer(s_ResourceManager->State, ShaderData->ShaderResources.UniformBuffer)->Handle;
        DescriptorBufferInfo.offset = MaterialData->InstanceUBOOffset;
        DescriptorBufferInfo.range = Shader->InstanceUBOStride;

        DescriptorWriteInfo = {};
        DescriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWriteInfo.descriptorCount = 1;
        DescriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        DescriptorWriteInfo.dstSet = MaterialData->LocalDescriptorSets[s_Context.CurrentSwapchainImageIndex];
        DescriptorWriteInfo.dstBinding = BindingIndex;
        DescriptorWriteInfo.dstArrayElement = 0;
        DescriptorWriteInfo.pBufferInfo = &DescriptorBufferInfo;

        WriteInfos[WriteCount] = DescriptorWriteInfo;
        MaterialData->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex]++;
        WriteCount++;
    }

    BindingIndex++;

    // Sampler
    if (MaterialData->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex] == KRAFT_INVALID_ID_UINT8)
    {
        for (int i = 0; i < MaterialInstance->Textures.Length; i++)
        {
            Handle<Texture> Resource = MaterialInstance->Textures[i];
            VulkanTexture*  VkTexture = VulkanResourceManagerApi::GetTexture(s_ResourceManager->State, Resource);

            KASSERT(VkTexture->Image);
            KASSERT(VkTexture->View);

            DescriptorImageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            DescriptorImageInfo[i].imageView = VkTexture->View;
            DescriptorImageInfo[i].sampler = VkTexture->Sampler;
        }

        DescriptorWriteInfo = {};
        DescriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWriteInfo.descriptorCount = (uint32)MaterialInstance->Textures.Length;
        DescriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        DescriptorWriteInfo.dstSet = MaterialData->LocalDescriptorSets[s_Context.CurrentSwapchainImageIndex];
        DescriptorWriteInfo.dstBinding = BindingIndex;
        DescriptorWriteInfo.dstArrayElement = 0;
        DescriptorWriteInfo.pImageInfo = DescriptorImageInfo;

        WriteInfos[WriteCount] = DescriptorWriteInfo;
        MaterialData->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex]++;
        WriteCount++;
    }

    BindingIndex++;

    if (WriteCount > 0)
    {
        vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, WriteCount, WriteInfos, 0, 0);
    }

    vkCmdBindDescriptorSets(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, ShaderData->PipelineLayout, 2, 1, &MaterialData->LocalDescriptorSets[s_Context.CurrentSwapchainImageIndex], 0, 0);
}

void VulkanRendererBackend::DrawGeometryData(uint32 GeometryID)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, s_Context.ActiveCommandBuffer);
    VulkanGeometryData   GeometryData = s_Context.Geometries[GeometryID];
    VkDeviceSize         Offsets[1] = { GeometryData.VertexBufferOffset };

    VulkanBuffer* VertexBuffer = VulkanResourceManagerApi::GetBuffer(s_ResourceManager->State, s_Context.VertexBuffer);
    VulkanBuffer* IndexBuffer = VulkanResourceManagerApi::GetBuffer(s_ResourceManager->State, s_Context.IndexBuffer);

    vkCmdBindVertexBuffers(GPUCmdBuffer->Resource, 0, 1, &VertexBuffer->Handle, Offsets);
    vkCmdBindIndexBuffer(GPUCmdBuffer->Resource, IndexBuffer->Handle, GeometryData.IndexBufferOffset, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(GPUCmdBuffer->Resource, GeometryData.IndexCount, 1, 0, 0, 0);
}

static bool UploadDataToGPU(VulkanContext* Context, Handle<Buffer> DstBuffer, uint32 DstBufferOffset, const void* Data, uint32 Size)
{
    GPUBuffer StagingBuffer = ResourceManager->CreateTempBuffer(Size);
    MemCpy(StagingBuffer.Ptr, Data, Size);

    ResourceManager->UploadBuffer({
        .DstBuffer = DstBuffer,
        .SrcBuffer = StagingBuffer.GPUBuffer,
        .SrcSize = Size,
        .DstOffset = DstBufferOffset,
        .SrcOffset = StagingBuffer.Offset,
    });

    return true;
}

bool VulkanRendererBackend::CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize)
{
    VulkanGeometryData* GeometryData = &s_Context.Geometries[Geometry->ID];
    Geometry->InternalID = Geometry->ID;

    uint32 Size = VertexSize * VertexCount;
    GeometryData->VertexSize = VertexSize;
    GeometryData->VertexCount = VertexCount;
    GeometryData->VertexBufferOffset = s_Context.CurrentVertexBufferOffset;
    s_Context.CurrentVertexBufferOffset += Size;

    UploadDataToGPU(&s_Context, s_Context.VertexBuffer, GeometryData->VertexBufferOffset, Vertices, Size);

    if (Indices && IndexCount > 0)
    {
        Size = IndexSize * IndexCount;
        GeometryData->IndexSize = IndexSize;
        GeometryData->IndexCount = IndexCount;
        GeometryData->IndexBufferOffset = s_Context.CurrentIndexBufferOffset;
        s_Context.CurrentIndexBufferOffset += Size;

        UploadDataToGPU(&s_Context, s_Context.IndexBuffer, GeometryData->IndexBufferOffset, Indices, Size);
    }

    return true;
}

void VulkanRendererBackend::DestroyGeometry(Geometry* Geometry)
{
    if (!Geometry || Geometry->InternalID == KRAFT_INVALID_ID)
        return;

    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);

    VulkanGeometryData* GeometryData = &s_Context.Geometries[Geometry->InternalID];

    MemZero(GeometryData, sizeof(VulkanGeometryData));
    GeometryData->ID = KRAFT_INVALID_ID;
}

void VulkanRendererBackend::BeginRenderPass(Handle<CommandBuffer> CmdBufferHandle, Handle<RenderPass> PassHandle)
{
    // Handle<CommandBuffer> CmdBuffer = ActiveCmdBuffer = this->CommandBuffers[s_Context.CurrentSwapchainImageIndex];
    s_Context.ActiveCommandBuffer = CmdBufferHandle;
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, CmdBufferHandle);
    VulkanResetCommandBuffer(GPUCmdBuffer);
    VulkanBeginCommandBuffer(GPUCmdBuffer, false, false, false);

    VulkanRenderPass* GPURenderPass = VulkanResourceManagerApi::GetRenderPass(s_ResourceManager->State, PassHandle);
    RenderPass*       RenderPass = s_ResourceManager->GetRenderPassMetadata(PassHandle);

    VkRenderPassBeginInfo BeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    BeginInfo.framebuffer = GPURenderPass->Framebuffer.Handle;
    BeginInfo.renderPass = GPURenderPass->Handle;
    BeginInfo.renderArea.offset.x = 0;
    BeginInfo.renderArea.offset.y = 0;
    BeginInfo.renderArea.extent.width = (uint32)RenderPass->Dimensions.x;
    BeginInfo.renderArea.extent.height = (uint32)RenderPass->Dimensions.y;

    VkClearValue ClearValues[32] = {};
    uint32       ColorValuesIndex = 0;
    for (int i = 0; i < RenderPass->ColorTargets.Size(); i++)
    {
        const ColorTarget& Target = RenderPass->ColorTargets[i];
        ClearValues[i].color.float32[0] = Target.ClearColor.r;
        ClearValues[i].color.float32[1] = Target.ClearColor.g;
        ClearValues[i].color.float32[2] = Target.ClearColor.b;
        ClearValues[i].color.float32[3] = Target.ClearColor.a;

        ColorValuesIndex++;
    }

    ClearValues[ColorValuesIndex].depthStencil.depth = RenderPass->DepthTarget.Depth;
    ClearValues[ColorValuesIndex].depthStencil.stencil = RenderPass->DepthTarget.Stencil;
    ColorValuesIndex++;

    BeginInfo.clearValueCount = ColorValuesIndex;
    BeginInfo.pClearValues = ClearValues;

    // TODO: Figure out how to handle "Contents" parameter
    vkCmdBeginRenderPass(GPUCmdBuffer->Resource, &BeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    GPUCmdBuffer->State = VULKAN_COMMAND_BUFFER_STATE_IN_RENDER_PASS;

    VkViewport Viewport = {};
    Viewport.width = RenderPass->Dimensions.x;
    Viewport.height = RenderPass->Dimensions.y;
    Viewport.maxDepth = 1.0f;

    vkCmdSetViewport(GPUCmdBuffer->Resource, 0, 1, &Viewport);

    VkRect2D Scissor = {};
    Scissor.extent = { (uint32)RenderPass->Dimensions.x, (uint32)RenderPass->Dimensions.y };

    vkCmdSetScissor(GPUCmdBuffer->Resource, 0, 1, &Scissor);

    VulkanRendererBackendState.BuffersToSubmit[VulkanRendererBackendState.BuffersToSubmitNum] = GPUCmdBuffer->Resource;
    VulkanRendererBackendState.BuffersToSubmitNum++;
}

void VulkanRendererBackend::EndRenderPass(Handle<CommandBuffer> CmdBufferHandle, Handle<RenderPass> PassHandle)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_ResourceManager->State, CmdBufferHandle);
    vkCmdEndRenderPass(GPUCmdBuffer->Resource);

    // VkMemoryBarrier2 MemoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    // MemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // MemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    // MemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
    // MemoryBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    // VkDependencyInfo DependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    // DependencyInfo.memoryBarrierCount = 1;
    // DependencyInfo.pMemoryBarriers = &MemoryBarrier;

    // vkCmdPipelineBarrier2(GPUCmdBuffer->Resource, &DependencyInfo);

    VulkanEndCommandBuffer(GPUCmdBuffer);
    VulkanSetCommandBufferSubmitted(GPUCmdBuffer);
}

#ifdef KRAFT_RENDERER_DEBUG
static VkBool32
DebugUtilsMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        {
            KERROR("%s", pCallbackData->pMessage);
        }
        break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        {
            KWARN("%s", pCallbackData->pMessage);
        }
        break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        {
            KDEBUG("%s", pCallbackData->pMessage);
        }
        break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        {
            KDEBUG("%s", pCallbackData->pMessage);
        }
        break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: break;
    }

    return VK_FALSE;
}

static void SetObjectName(uint64 Object, VkObjectType ObjectType, const char* Name)
{
    VkDebugUtilsObjectNameInfoEXT NameInfo = {};
    NameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    NameInfo.objectType = ObjectType;
    NameInfo.objectHandle = Object;
    NameInfo.pObjectName = Name;
    vkSetDebugUtilsObjectNameEXT(s_Context.LogicalDevice.Handle, &NameInfo);
}
#endif

bool createBuffers()
{
    const uint64 VertexBufferSize = sizeof(Vertex3D) * 1024 * 256;
    s_Context.VertexBuffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalVertexBuffer",
        .Size = VertexBufferSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_VERTEX_BUFFER,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
    });

    const uint64 IndexBufferSize = sizeof(uint32) * 1024 * 256;
    s_Context.IndexBuffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalIndexBuffer",
        .Size = IndexBufferSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_INDEX_BUFFER,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
    });

    return true;
}

void createCommandBuffers()
{
    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        s_Context.GraphicsCommandBuffers[i] = s_ResourceManager->CreateCommandBuffer({
            .DebugName = "MainCmdBuffer",
            .CommandPool = s_Context.GraphicsCommandPool,
            .Primary = true,
        });
    }

    KDEBUG("[VulkanRendererBackend::Init]: Graphics command buffers created");
}

void destroyCommandBuffers()
{
    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        if (s_Context.GraphicsCommandBuffers[i])
        {
            s_ResourceManager->DestroyCommandBuffer(s_Context.GraphicsCommandBuffers[i]);
        }
    }
}

void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* renderPass)
{
    if (!swapchain->Framebuffers)
    {
        arrsetlen(swapchain->Framebuffers, swapchain->ImageCount);
        MemZero(swapchain->Framebuffers, sizeof(VulkanFramebuffer) * swapchain->ImageCount);
    }

    VulkanResourceManagerApi::GetTexture(s_ResourceManager->State, swapchain->DepthAttachment);

    VkImageView Attachments[] = { 0, VulkanResourceManagerApi::GetTexture(s_ResourceManager->State, swapchain->DepthAttachment)->View };
    for (uint32 i = 0; i < swapchain->ImageCount; ++i)
    {
        if (swapchain->Framebuffers[i].Handle)
        {
            VulkanDestroyFramebuffer(&s_Context, &swapchain->Framebuffers[i]);
        }

        Attachments[0] = swapchain->ImageViews[i];
        VulkanCreateFramebuffer(&s_Context, s_Context.FramebufferWidth, s_Context.FramebufferHeight, renderPass, Attachments, KRAFT_C_ARRAY_SIZE(Attachments), &swapchain->Framebuffers[i]);
    }
}

void destroyFramebuffers(VulkanSwapchain* swapchain)
{
    if (swapchain->Framebuffers)
    {
        for (int i = 0; i < arrlen(swapchain->Framebuffers); ++i)
        {
            VulkanDestroyFramebuffer(&s_Context, &swapchain->Framebuffers[i]);
        }
    }

    arrfree(swapchain->Framebuffers);
}

bool instanceLayerSupported(const char* layer, VkLayerProperties* layerProperties, uint32 count)
{
    for (uint32 j = 0; j < count; ++j)
    {
        if (strcmp(layerProperties[j].layerName, layer) == 0)
        {
            return true;
        }
    }

    return false;
}
}

namespace kraft {

using namespace renderer;

}