#include "kraft_vulkan_backend.h"

#include <volk/volk.h>
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include <containers/kraft_carray.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_allocators.h>
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
#include <renderer/vulkan/kraft_vulkan_renderpass.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>
#include <renderer/vulkan/kraft_vulkan_shader.h>
#include <renderer/vulkan/kraft_vulkan_swapchain.h>
#include <shaders/includes/kraft_shader_includes.h>

#include <kraft_types.h>
#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>
#include <shaderfx/kraft_shaderfx_types.h>

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

VulkanContext* VulkanRendererBackend::Context()
{
    return &s_Context;
}

void VulkanRendererBackend::SetDeviceData(GPUDevice* device)
{
    VkPhysicalDeviceProperties* device_properties = &s_Context.PhysicalDevice.Properties;
    device->min_storage_buffer_alignment = device_properties->limits.minStorageBufferOffsetAlignment;
    device->min_uniform_buffer_alignment = device_properties->limits.minUniformBufferOffsetAlignment;
    device->supports_device_local_host_visible = s_Context.PhysicalDevice.SupportsDeviceLocalHostVisible;
}

static bool createBuffers();
static void createCommandBuffers();
static void destroyCommandBuffers();
static void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* renderPass);
static void destroyFramebuffers(VulkanSwapchain* swapchain);

struct VulkanRendererBackendStateT
{
    VkCommandBuffer BuffersToSubmit[16];
    uint32          BuffersToSubmitNum = 0;
} VulkanRendererBackendState;

bool VulkanRendererBackend::Init(ArenaAllocator* Arena, RendererOptions* Opts)
{
    KRAFT_VK_CHECK(volkInitialize());
    s_Context = VulkanContext{
        .Options = Opts,
    };

    s_ResourceManager = ResourceManager;

    s_Context.AllocationCallbacks = nullptr;
    s_Context.FramebufferWidth = Platform::GetWindow()->Width;
    s_Context.FramebufferHeight = Platform::GetWindow()->Height;

    for (int i = 0; i < KRAFT_VULKAN_MAX_GEOMETRIES; i++)
    {
        s_Context.Geometries[i].ID = KRAFT_INVALID_ID;
    }

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pApplicationName = "kraft_vk_app";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "kraft_vk_engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);

    VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    // Instance level extensions
    const char* Extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#if defined(KRAFT_PLATFORM_WINDOWS)
        "VK_KHR_win32_surface",
#elif defined(KRAFT_PLATFORM_MACOS)
        "VK_EXT_metal_surface",
#elif defined(KRAFT_PLATFORM_LINUX)
        "VK_KHR_xcb_surface",
#endif
#ifdef KRAFT_RENDERER_DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

    KINFO("[VulkanRendererBackend::Init]: Loading the following debug extensions:");
    for (int i = 0; i < KRAFT_C_ARRAY_SIZE(Extensions); ++i)
    {
        KINFO("[VulkanRendererBackend::Init]: %s", Extensions[i]);
    }

    instanceCreateInfo.enabledExtensionCount = KRAFT_C_ARRAY_SIZE(Extensions);
    instanceCreateInfo.ppEnabledExtensionNames = Extensions;

    // Layers
    const char* InstanceLayers[] = {
#ifdef KRAFT_RENDERER_DEBUG
        // Vulkan validation layer
        "VK_LAYER_KHRONOS_validation",
#endif
    };

    // Make sure all the required layers are available
    uint32 AvailableLayersCount = 0;
    KRAFT_VK_CHECK(vkEnumerateInstanceLayerProperties(&AvailableLayersCount, 0));

    auto AvailableLayerProperties = ArenaPushArray(Arena, VkLayerProperties, AvailableLayersCount);
    KRAFT_VK_CHECK(vkEnumerateInstanceLayerProperties(&AvailableLayersCount, AvailableLayerProperties));

    for (int i = 0; i < KRAFT_C_ARRAY_SIZE(InstanceLayers); ++i)
    {
        bool Found = false;
        for (uint32 j = 0; j < AvailableLayersCount; ++j)
        {
            if (strcmp(AvailableLayerProperties[j].layerName, InstanceLayers[i]) == 0)
            {
                Found = true;
            }
        }

        if (!Found)
        {
            KERROR("[VulkanRendererBackend::Init]: %s layer not found", InstanceLayers[i]);
            return false;
        }
    }

    instanceCreateInfo.enabledLayerCount = (uint32)KRAFT_C_ARRAY_SIZE(InstanceLayers);
    instanceCreateInfo.ppEnabledLayerNames = InstanceLayers;

    KRAFT_VK_CHECK(vkCreateInstance(&instanceCreateInfo, s_Context.AllocationCallbacks, &s_Context.Instance));

    ArenaPop(Arena, sizeof(VkLayerProperties) * AvailableLayersCount);

    volkLoadInstance(s_Context.Instance);

#ifdef KRAFT_RENDERER_DEBUG
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
    VulkanPhysicalDeviceRequirements Requirements = {};
    Requirements.Compute = true;
    Requirements.Graphics = true;
    Requirements.Present = true;
    Requirements.Transfer = true;
    Requirements.DepthBuffer = true;
    Requirements.DeviceExtensionsCount = 2;
    // We add + 1 for VK_KHR_portability_subset
    u64 max_device_extensions_count = 3;
    Requirements.DeviceExtensions = ArenaPushArray(Arena, const char*, max_device_extensions_count);

    Requirements.DeviceExtensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    Requirements.DeviceExtensions[1] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;

    GLFWwindow* window = Platform::GetWindow()->PlatformWindowHandle;
    KRAFT_VK_CHECK(glfwCreateWindowSurface(s_Context.Instance, window, s_Context.AllocationCallbacks, &s_Context.Surface));

    KSUCCESS("[VulkanRendererBackend::Init]: Successfully created VkSurface");

    VulkanSelectPhysicalDevice(Arena, &s_Context, &Requirements);
    VulkanCreateLogicalDevice(Arena, &s_Context, &Requirements);

    s_Context.GraphicsCommandPool = s_ResourceManager->CreateCommandPool({
        .DebugName = "PrimaryGfxCmdPool",
        .QueueFamilyIndex = (uint32)s_Context.PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex,
        .Flags = COMMAND_POOL_CREATE_FLAGS_RESET_COMMAND_BUFFER_BIT,
    });

    KDEBUG("[VulkanRendererBackend::Init]: Graphics command pool created");
    ArenaPop(Arena, sizeof(const char*) * max_device_extensions_count);

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

    VulkanCreateSwapchain(&s_Context, s_Context.FramebufferWidth, s_Context.FramebufferHeight, s_Context.Options->VSync);
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

    s_Context.DefaultTextureSampler = ResourceManager->CreateTextureSampler({});
    KASSERT(s_Context.DefaultTextureSampler.IsInvalid() == false);

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
        VkDescriptorSetLayoutBinding GlobalDataLayoutBinding[3] = {};

        // Binding #0: Global Shader Data - Projection, View, Camera, etc
        GlobalDataLayoutBinding[0].binding = 0;
        GlobalDataLayoutBinding[0].descriptorCount = 1;
        GlobalDataLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        GlobalDataLayoutBinding[0].pImmutableSamplers = 0;
        GlobalDataLayoutBinding[0].stageFlags = VK_SHADER_STAGE_ALL;

        // Binding #1: Default texture Sampler
        GlobalDataLayoutBinding[1].binding = 1;
        GlobalDataLayoutBinding[1].descriptorCount = 1;
        GlobalDataLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        GlobalDataLayoutBinding[1].pImmutableSamplers = 0;
        GlobalDataLayoutBinding[1].stageFlags = VK_SHADER_STAGE_ALL;

        // Binding #2: Global materials array
        GlobalDataLayoutBinding[2].binding = 2;
        GlobalDataLayoutBinding[2].descriptorCount = 1;
        GlobalDataLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        GlobalDataLayoutBinding[2].pImmutableSamplers = 0;
        GlobalDataLayoutBinding[2].stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo GlobalDataLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        GlobalDataLayoutCreateInfo.bindingCount = KRAFT_C_ARRAY_SIZE(GlobalDataLayoutBinding);
        GlobalDataLayoutCreateInfo.pBindings = &GlobalDataLayoutBinding[0];
        GlobalDataLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &GlobalDataLayoutCreateInfo, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        // Set 1: This Set is unused
        VkDescriptorSetLayoutBinding __unused_set_layout_binding = {};
        __unused_set_layout_binding.binding = 0;
        __unused_set_layout_binding.descriptorCount = 1;
        __unused_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        __unused_set_layout_binding.pImmutableSamplers = 0;
        __unused_set_layout_binding.stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo __unused_set_layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        __unused_set_layout_create_info.bindingCount = 1;
        __unused_set_layout_create_info.pBindings = &__unused_set_layout_binding;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &__unused_set_layout_create_info, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        // Set 2: Textures
        // Binding #0: Global texture array
        VkDescriptorSetLayoutBinding TextureDataLayoutBinding;
        TextureDataLayoutBinding.binding = 0;
        TextureDataLayoutBinding.descriptorCount = KRAFT_RENDERER__MAX_GLOBAL_TEXTURES; // Number of global textures
        TextureDataLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        TextureDataLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        TextureDataLayoutBinding.pImmutableSamplers = 0;

        VkDescriptorBindingFlags BindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        VkDescriptorSetLayoutBindingFlagsCreateInfo SetBindingFlags;
        SetBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        SetBindingFlags.bindingCount = 1;
        SetBindingFlags.pBindingFlags = &BindingFlags;

        VkDescriptorSetLayoutCreateInfo TextureSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        TextureSetLayoutCreateInfo.bindingCount = 1;
        TextureSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        TextureSetLayoutCreateInfo.pBindings = &TextureDataLayoutBinding;
        TextureSetLayoutCreateInfo.pNext = &SetBindingFlags;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &TextureSetLayoutCreateInfo, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        uint32                                             TextureCount = KRAFT_RENDERER__MAX_GLOBAL_TEXTURES;
        VkDescriptorSetVariableDescriptorCountAllocateInfo SetAllocateCountInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        SetAllocateCountInfo.descriptorSetCount = 1;
        SetAllocateCountInfo.pDescriptorCounts = &TextureCount;

        VkDescriptorSetAllocateInfo SetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        SetAllocateInfo.pNext = &SetAllocateCountInfo;
        SetAllocateInfo.descriptorPool = s_Context.GlobalDescriptorPool;
        SetAllocateInfo.descriptorSetCount = 1;
        SetAllocateInfo.pSetLayouts = &s_Context.DescriptorSetLayouts[2];

        KRAFT_VK_CHECK(vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &SetAllocateInfo, &s_Context.GlobalTexturesDescriptorSet));

        // Create the uniform buffer for data upload
        uint64 ExtraMemoryFlags = s_Context.PhysicalDevice.SupportsDeviceLocalHostVisible ? MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL : 0;
        s_Context.GlobalUniformBuffer = ResourceManager->CreateBuffer({
            .DebugName = "GlobalUniformBuffer",
            .Size = (uint64)math::AlignUp(sizeof(GlobalShaderData), s_Context.PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment),
            .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
            .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | ExtraMemoryFlags,
            .MapMemory = true,
        });

        // Map the buffer memory
        s_Context.GlobalUniformBufferMemory = ResourceManager->GetBufferData(s_Context.GlobalUniformBuffer);
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

    for (uint32 i = 0; i < s_Context.DescriptorSetLayoutsCount; i++)
    {
        vkDestroyDescriptorSetLayout(s_Context.LogicalDevice.Handle, s_Context.DescriptorSetLayouts[i], s_Context.AllocationCallbacks);
    }

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

    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
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
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
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
    const shaderfx::ShaderEffect& Effect = Shader->ShaderEffect;
    uint64                        RenderPassesCount = Effect.RenderPasses.Length;

    const shaderfx::RenderPassDefinition& Pass = Effect.RenderPasses[PassIndex];
    VkGraphicsPipelineCreateInfo          PipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    // Shader stages
    uint64                ShaderStagesCount = Pass.ShaderStages.Length;
    auto                  PipelineShaderStageCreateInfos = Array<VkPipelineShaderStageCreateInfo>(ShaderStagesCount);
    Array<VkShaderModule> ShaderModules(ShaderStagesCount);
    for (int j = 0; j < ShaderStagesCount; j++)
    {
        const shaderfx::RenderPassDefinition::ShaderDefinition& Shader = Pass.ShaderStages[j];
        VkShaderModule                                          ShaderModule;
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
        const shaderfx::VertexInputBinding& InputBinding = Pass.VertexLayout->InputBindings[j];
        InputBindingsDesc[j].binding = InputBinding.Binding;
        InputBindingsDesc[j].inputRate = InputBinding.InputRate == VertexInputRate::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        InputBindingsDesc[j].stride = InputBinding.Stride;
    }

    uint64 AttributesCount = Pass.VertexLayout->Attributes.Length;
    auto   AttributesDesc = Array<VkVertexInputAttributeDescription>(AttributesCount);
    for (int j = 0; j < AttributesCount; j++)
    {
        const shaderfx::VertexAttribute& VertexAttribute = Pass.VertexLayout->Attributes[j];
        AttributesDesc[j].location = VertexAttribute.Location;
        AttributesDesc[j].format = ToVulkanFormat(VertexAttribute.Format);
        AttributesDesc[j].binding = VertexAttribute.Binding;
        AttributesDesc[j].offset = VertexAttribute.Offset;
    }

    // Shader resources
    uint64 ResourceBindingsCount = Pass.Resources != nullptr ? Pass.Resources->ResourceBindings.Length : 0;
    uint64 ConstantBuffersCount = Pass.ConstantBuffers->Fields.Length;

    // Constant Buffers
    uint32 ConstantBufferSize = 0;
    auto   PushConstantRanges = Array<VkPushConstantRange>(ConstantBuffersCount);
    for (int j = 0; j < ConstantBuffersCount; j++)
    {
        const shaderfx::ConstantBufferEntry& Entry = Pass.ConstantBuffers->Fields[j];
        uint32                               AlignedSize = (uint32)math::AlignUp(ShaderDataType::SizeOf(Entry.Type), 4);
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

    VkDescriptorSetLayout DescriptorSetLayouts[KRAFT_VULKAN_MAX_DESCRIPTOR_SETS_PER_SHADER] = { 0 };
    int                   LayoutsCount = 0;
    while (LayoutsCount < s_Context.DescriptorSetLayoutsCount)
    {
        DescriptorSetLayouts[LayoutsCount++] = s_Context.DescriptorSetLayouts[LayoutsCount];
    }

    KASSERT(LayoutsCount <= KRAFT_VULKAN_NUM_INBUILT_DESCRIPTOR_SETS);

    VulkanShader* internal_shader_data = (VulkanShader*)Malloc(sizeof(VulkanShader), MEMORY_TAG_RENDERER, true);

    // Collect bindings into sets
    shaderfx::ResourceBinding bindings_by_set[KRAFT_VULKAN_MAX_DESCRIPTOR_SETS_PER_SHADER][KRAFT_VULKAN_MAX_BINDINGS_PER_SET] = {};
    uint32                    max_set_number = 0;
    for (uint32 i = 0; i < Shader->ShaderEffect.GlobalResources.Length; i++)
    {
        for (uint32 j = 0; j < Shader->ShaderEffect.GlobalResources[i].ResourceBindings.Length; j++)
        {
            const shaderfx::ResourceBinding& binding = Shader->ShaderEffect.GlobalResources[i].ResourceBindings[j];
            bindings_by_set[binding.Set][binding.Binding] = binding;

            max_set_number = binding.Set > max_set_number ? binding.Set : max_set_number;
        }
    }

    for (uint32 i = 0; i < Shader->ShaderEffect.LocalResources.Length; i++)
    {
        for (uint32 j = 0; j < Shader->ShaderEffect.LocalResources[i].ResourceBindings.Length; j++)
        {
            const shaderfx::ResourceBinding& binding = Shader->ShaderEffect.LocalResources[i].ResourceBindings[j];
            bindings_by_set[binding.Set][binding.Binding] = binding;

            max_set_number = binding.Set > max_set_number ? binding.Set : max_set_number;
        }
    }

    KASSERT(max_set_number < KRAFT_VULKAN_MAX_DESCRIPTOR_SETS_PER_SHADER);

    // Do we have any shader specific local resources?
    // We start from the set after the inbuilt ones
    for (uint32 set = KRAFT_VULKAN_NUM_INBUILT_DESCRIPTOR_SETS; set <= max_set_number; set++)
    {
        auto bindings = Array<VkDescriptorSetLayoutBinding>();
        for (uint32 binding_idx = 0; binding_idx < 16; binding_idx++)
        {
            const shaderfx::ResourceBinding& binding = bindings_by_set[set][binding_idx];
            // If these don't match, the binding isn't set
            if (binding.Set == set)
            {
                auto binding_ref = bindings.Push(VkDescriptorSetLayoutBinding());
                binding_ref->binding = binding.Binding;
                binding_ref->descriptorCount = 1;
                binding_ref->descriptorType = ToVulkanResourceType(binding.Type);
                binding_ref->pImmutableSamplers = 0;
                binding_ref->stageFlags = ToVulkanShaderStageFlags(binding.Stage);
            }
        }

        VkDescriptorSetLayoutCreateInfo descriptor_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descriptor_create_info.bindingCount = (uint32)bindings.Length;
        descriptor_create_info.pBindings = bindings.Data();

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &descriptor_create_info, s_Context.AllocationCallbacks, &internal_shader_data->descriptor_set_layouts[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)]
        ));

        VkDescriptorSetAllocateInfo set_allocate_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        set_allocate_info.descriptorPool = s_Context.GlobalDescriptorPool;
        set_allocate_info.descriptorSetCount = 1;
        set_allocate_info.pSetLayouts = &internal_shader_data->descriptor_set_layouts[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)];

        vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &set_allocate_info, &internal_shader_data->descriptor_sets[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)]);

        DescriptorSetLayouts[set] = internal_shader_data->descriptor_set_layouts[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)];
        LayoutsCount = set + 1;
    }

    VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.setLayoutCount = LayoutsCount;
    LayoutCreateInfo.pSetLayouts = &DescriptorSetLayouts[0];
    LayoutCreateInfo.pushConstantRangeCount = 1;
    auto PCRange = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = 128,
    };
    LayoutCreateInfo.pPushConstantRanges = &PCRange;

    VkPipelineLayout PipelineLayout;
    KRAFT_VK_CHECK(vkCreatePipelineLayout(s_Context.LogicalDevice.Handle, &LayoutCreateInfo, s_Context.AllocationCallbacks, &PipelineLayout));
    KASSERT(PipelineLayout);

    String DebugPipelineLayoutName = Shader->ShaderEffect.Name + "PipelineLayout";
    s_Context.SetObjectName((uint64)PipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, *DebugPipelineLayoutName);

    PipelineCreateInfo.layout = PipelineLayout;

    PipelineCreateInfo.stageCount = (uint32)PipelineShaderStageCreateInfos.Length;
    PipelineCreateInfo.pStages = &PipelineShaderStageCreateInfos[0];
    PipelineCreateInfo.pTessellationState = 0;

    // PipelineCreateInfo.renderPass = s_Context.MainRenderPass.Handle;
    PipelineCreateInfo.renderPass = RenderPassHandle ? VulkanResourceManagerApi::GetRenderPass(RenderPassHandle)->Handle : s_Context.MainRenderPass.Handle;
    PipelineCreateInfo.subpass = 0;
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;

    VkPipeline Pipeline;
    KRAFT_VK_CHECK(vkCreateGraphicsPipelines(s_Context.LogicalDevice.Handle, VK_NULL_HANDLE, 1, &PipelineCreateInfo, s_Context.AllocationCallbacks, &Pipeline));
    KASSERT(Pipeline);

    String DebugPipelineName = Shader->ShaderEffect.Name + "Pipeline";
    s_Context.SetObjectName((uint64)Pipeline, VK_OBJECT_TYPE_PIPELINE, *DebugPipelineName);

    // Map the buffer memory
    internal_shader_data->Pipeline = Pipeline;
    internal_shader_data->PipelineLayout = PipelineLayout;
    Shader->RendererData = internal_shader_data;

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
        VulkanShader*    VulkanShaderData = (VulkanShader*)Shader->RendererData;
        VkPipeline       Pipeline = VulkanShaderData->Pipeline;
        VkPipelineLayout PipelineLayout = VulkanShaderData->PipelineLayout;

        vkDestroyPipeline(s_Context.LogicalDevice.Handle, Pipeline, s_Context.AllocationCallbacks);
        vkDestroyPipelineLayout(s_Context.LogicalDevice.Handle, PipelineLayout, s_Context.AllocationCallbacks);

        Free(Shader->RendererData, sizeof(VulkanShader), MEMORY_TAG_RENDERER);
        Shader->RendererData = 0;
    }
}

void VulkanRendererBackend::SetGlobalShaderData(GlobalShaderData* Data)
{
    MemCpy(s_Context.GlobalUniformBufferMemory, Data, sizeof(GlobalShaderData));
}

void VulkanRendererBackend::UseShader(const Shader* Shader)
{
    VulkanShader*        VulkanShaderData = (VulkanShader*)Shader->RendererData;
    VkPipeline           Pipeline = VulkanShaderData->Pipeline;
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);

    vkCmdBindPipeline(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
}

void VulkanRendererBackend::ApplyGlobalShaderProperties(Shader* Shader, Handle<Buffer> GlobalUBOBuffer, Handle<Buffer> GlobalMaterialsBuffer)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanShader*        ShaderData = (VulkanShader*)Shader->RendererData;

    VkBuffer               GPUBuffer = VulkanResourceManagerApi::GetBuffer(GlobalUBOBuffer.IsInvalid() ? s_Context.GlobalUniformBuffer : GlobalUBOBuffer)->Handle;
    VkDescriptorBufferInfo GlobalDataBufferInfo = {};
    GlobalDataBufferInfo.buffer = GPUBuffer;
    GlobalDataBufferInfo.offset = 0;
    GlobalDataBufferInfo.range = VK_WHOLE_SIZE;

    uint32               count = 0;
    VkWriteDescriptorSet DescriptorWriteInfo[3] = {};
    DescriptorWriteInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWriteInfo[0].descriptorCount = 1;
    DescriptorWriteInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    DescriptorWriteInfo[0].dstBinding = 0;
    DescriptorWriteInfo[0].dstArrayElement = 0;
    DescriptorWriteInfo[0].pBufferInfo = &GlobalDataBufferInfo;
    count++;

    VkDescriptorImageInfo ImageInfo;
    ImageInfo.sampler = VulkanResourceManagerApi::GetTextureSampler(s_Context.DefaultTextureSampler)->Sampler;
    ImageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageInfo.imageView = VK_NULL_HANDLE;

    DescriptorWriteInfo[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWriteInfo[1].descriptorCount = 1;
    DescriptorWriteInfo[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    DescriptorWriteInfo[1].dstBinding = 1;
    DescriptorWriteInfo[1].dstArrayElement = 0;
    DescriptorWriteInfo[1].pImageInfo = &ImageInfo;
    count++;

    VulkanBuffer*          VkBuffer = VulkanResourceManagerApi::GetBuffer(GlobalMaterialsBuffer);
    VkDescriptorBufferInfo GlobalMaterialsBufferInfo = {};
    if (VkBuffer)
    {
        GlobalMaterialsBufferInfo.buffer = VkBuffer->Handle;
        GlobalMaterialsBufferInfo.range = VK_WHOLE_SIZE;

        DescriptorWriteInfo[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWriteInfo[2].descriptorCount = 1;
        DescriptorWriteInfo[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        DescriptorWriteInfo[2].dstBinding = 2;
        DescriptorWriteInfo[2].dstArrayElement = 0;
        DescriptorWriteInfo[2].pBufferInfo = &GlobalMaterialsBufferInfo;
        count++;
    }

    vkCmdPushDescriptorSetKHR(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, ShaderData->PipelineLayout, 0, count, &DescriptorWriteInfo[0]);
    vkCmdBindDescriptorSets(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, ShaderData->PipelineLayout, 2, 1, &s_Context.GlobalTexturesDescriptorSet, 0, nullptr);
}

void VulkanRendererBackend::ApplyLocalShaderProperties(Shader* Shader, void* Data)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanShader*        ShaderData = (VulkanShader*)Shader->RendererData;
    vkCmdPushConstants(GPUCmdBuffer->Resource, ShaderData->PipelineLayout, VK_SHADER_STAGE_ALL, 0, 128, Data);
}

void VulkanRendererBackend::UpdateTextures(Array<Handle<Texture>> Textures)
{
    for (uint32 i = 0; i < Textures.Length; i++)
    {
        VulkanTexture*        Texture = VulkanResourceManagerApi::GetTexture(Textures[i]);
        VkDescriptorImageInfo ImageInfo = {};
        ImageInfo.imageView = Texture->View;
        ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet WriteData = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        WriteData.dstSet = s_Context.GlobalTexturesDescriptorSet;
        WriteData.dstBinding = 0;
        WriteData.dstArrayElement = Textures[i].GetIndex();
        WriteData.descriptorCount = 1;
        WriteData.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        WriteData.pImageInfo = &ImageInfo;

        vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, 1, &WriteData, 0, nullptr);
    }
}

void VulkanRendererBackend::DrawGeometryData(uint32 GeometryID)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanGeometryData   GeometryData = s_Context.Geometries[GeometryID];
    VkDeviceSize         Offsets[1] = { GeometryData.VertexBufferOffset };

    VulkanBuffer* VertexBuffer = VulkanResourceManagerApi::GetBuffer(s_Context.VertexBuffer);
    VulkanBuffer* IndexBuffer = VulkanResourceManagerApi::GetBuffer(s_Context.IndexBuffer);

    vkCmdBindVertexBuffers(GPUCmdBuffer->Resource, 0, 1, &VertexBuffer->Handle, Offsets);
    vkCmdBindIndexBuffer(GPUCmdBuffer->Resource, IndexBuffer->Handle, GeometryData.IndexBufferOffset, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(GPUCmdBuffer->Resource, GeometryData.IndexCount, 1, 0, 0, 0);
}

static bool UploadDataToGPU(VulkanContext* Context, Handle<Buffer> DstBuffer, uint32 DstBufferOffset, const void* Data, uint32 Size)
{
    BufferView StagingBuffer = ResourceManager->CreateTempBuffer(Size);
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
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(CmdBufferHandle);
    VulkanResetCommandBuffer(GPUCmdBuffer);
    VulkanBeginCommandBuffer(GPUCmdBuffer, false, false, false);

    VulkanRenderPass* GPURenderPass = VulkanResourceManagerApi::GetRenderPass(PassHandle);
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
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(CmdBufferHandle);
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

void VulkanRendererBackend::CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, uint32 set_idx, uint32 binding_idx)
{
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanShader*        shader_data = (VulkanShader*)shader->RendererData;

    VkBuffer               buffer_handle = VulkanResourceManagerApi::GetBuffer(buffer)->Handle;
    VkDescriptorBufferInfo info = {};
    info.buffer = buffer_handle;
    info.offset = 0;
    info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write_info = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write_info.descriptorCount = 1;
    write_info.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_info.dstBinding = binding_idx;
    write_info.pBufferInfo = &info;
    write_info.dstSet = shader_data->descriptor_sets[set_idx - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)];

    vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, 1, &write_info, 0, 0);
    vkCmdBindDescriptorSets(
        GPUCmdBuffer->Resource,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader_data->PipelineLayout,
        set_idx,
        1,
        &shader_data->descriptor_sets[set_idx - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)],
        0,
        nullptr
    );
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

    VulkanResourceManagerApi::GetTexture(swapchain->DepthAttachment);

    VkImageView Attachments[] = { 0, VulkanResourceManagerApi::GetTexture(swapchain->DepthAttachment)->View };
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
} // namespace kraft::renderer

namespace kraft {

using namespace renderer;

}