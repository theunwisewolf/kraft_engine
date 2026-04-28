#include <vulkan/vulkan_core.h>

#include <volk/volk.h>
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "kraft_includes.h"

namespace kraft::r {

#ifdef KRAFT_RENDERER_DEBUG
// PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag;
PFN_vkSetDebugUtilsObjectNameEXT KRAFT_vkSetDebugUtilsObjectNameEXT;
// PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin;
// PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd;
// PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert;
static VkBool32 DebugUtilsMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

static void SetObjectName(u64 object, VkObjectType type, const char* name);
#endif

static struct ResourceManager* s_ResourceManager = nullptr;
static VulkanContext s_Context = {};

VulkanContext* VulkanRendererBackend::Context() {
    return &s_Context;
}

void VulkanRendererBackend::SetDeviceData(GPUDevice* device) {
    VkPhysicalDeviceProperties* device_properties = &s_Context.PhysicalDevice.Properties;
    device->min_storage_buffer_alignment = device_properties->limits.minStorageBufferOffsetAlignment;
    device->min_uniform_buffer_alignment = device_properties->limits.minUniformBufferOffsetAlignment;
    device->supports_device_local_host_visible = s_Context.PhysicalDevice.SupportsDeviceLocalHostVisible;
}

static bool createBuffers();
static void createCommandBuffers();
static void destroyCommandBuffers();
static void imageBarrier(VkImage image, VkDependencyFlags dependency_flags, VulkanImageBarrierDescription description);

#if !KRAFT_ENABLE_VK_DYNAMIC_RENDERING
static void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* render_pass);
static void destroyFramebuffers(VulkanSwapchain* swapchain);
#endif

struct VulkanRendererBackendStateT {
    VkCommandBuffer BuffersToSubmit[16];
    u32 BuffersToSubmitNum = 0;
} VulkanRendererBackendState;

bool VulkanRendererBackend::Init(ArenaAllocator* arena, RendererOptions* renderer_options) {
    KRAFT_VK_CHECK(volkInitialize());
    s_Context = VulkanContext{
        .Options = renderer_options,
    };

    s_ResourceManager = ResourceManager;

    s_Context.AllocationCallbacks = nullptr;
    s_Context.FramebufferWidth = Platform::GetWindow()->Width;
    s_Context.FramebufferHeight = Platform::GetWindow()->Height;

    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.apiVersion = VK_API_VERSION_1_3;
    app_info.pApplicationName = "kraft_vk_app";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "kraft_vk_engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);

    VkInstanceCreateInfo instance_create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    // Instance level extensions
    const char* instance_extensions[] = {
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
    for (int i = 0; i < KRAFT_C_ARRAY_SIZE(instance_extensions); ++i) {
        KINFO("[VulkanRendererBackend::Init]: %s", instance_extensions[i]);
    }

    instance_create_info.enabledExtensionCount = KRAFT_C_ARRAY_SIZE(instance_extensions);
    instance_create_info.ppEnabledExtensionNames = instance_extensions;

    // Layers
    const char* instance_layers[] = {
#ifdef KRAFT_RENDERER_DEBUG
        // Vulkan validation layer
        "VK_LAYER_KHRONOS_validation",
#endif
    };

    // Make sure all the required layers are available
    u32 available_layers_count = 0;
    KRAFT_VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layers_count, 0));

    auto available_layer_properties = ArenaPushArray(arena, VkLayerProperties, available_layers_count);
    KRAFT_VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layers_count, available_layer_properties));

    for (i32 i = 0; i < KRAFT_C_ARRAY_SIZE(instance_layers); ++i) {
        bool found = false;
        for (u32 j = 0; j < available_layers_count; ++j) {
            if (strcmp(available_layer_properties[j].layerName, instance_layers[i]) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            KERROR("[VulkanRendererBackend::Init]: %s layer not found", instance_layers[i]);
            return false;
        }
    }

    instance_create_info.enabledLayerCount = (u32)KRAFT_C_ARRAY_SIZE(instance_layers);
    instance_create_info.ppEnabledLayerNames = instance_layers;

    KRAFT_VK_CHECK(vkCreateInstance(&instance_create_info, s_Context.AllocationCallbacks, &s_Context.Instance));

    ArenaPop(arena, sizeof(VkLayerProperties) * available_layers_count);

    volkLoadInstance(s_Context.Instance);

#ifdef KRAFT_RENDERER_DEBUG
    {
        // Setup vulkan debug callback messenger
        u32 log_level = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

        VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug_utils_messenger_create_info.messageSeverity = log_level;
        debug_utils_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_utils_messenger_create_info.pfnUserCallback = DebugUtilsMessenger;
        debug_utils_messenger_create_info.pUserData = 0;

        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_Context.Instance, "vkCreateDebugUtilsMessengerEXT");
        KRAFT_ASSERT_MESSAGE(func, "Failed to get function address for vkCreateDebugUtilsMessengerEXT");

        KRAFT_VK_CHECK(func(s_Context.Instance, &debug_utils_messenger_create_info, s_Context.AllocationCallbacks, &s_Context.DebugMessenger));
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
    requirements.DeviceExtensionsCount = 2;
    // We add + 1 for VK_KHR_portability_subset
    u64 max_device_extensions_count = 3;
    requirements.DeviceExtensions = ArenaPushArray(arena, const char*, max_device_extensions_count);

    requirements.DeviceExtensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    requirements.DeviceExtensions[1] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;

    GLFWwindow* window = Platform::GetWindow()->PlatformWindowHandle;
    KRAFT_VK_CHECK(glfwCreateWindowSurface(s_Context.Instance, window, s_Context.AllocationCallbacks, &s_Context.Surface));

    KSUCCESS("[VulkanRendererBackend::Init]: Successfully created VkSurface");

    VulkanSelectPhysicalDevice(arena, &s_Context, &requirements);
    VulkanCreateLogicalDevice(arena, &s_Context, &requirements);

    s_Context.GraphicsCommandPool = s_ResourceManager->CreateCommandPool({
        .DebugName = "PrimaryGfxCmdPool",
        .QueueFamilyIndex = (u32)s_Context.PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex,
        .Flags = COMMAND_POOL_CREATE_FLAGS_RESET_COMMAND_BUFFER_BIT,
    });

    KDEBUG("[VulkanRendererBackend::Init]: Graphics command pool created");
    ArenaPop(arena, sizeof(const char*) * max_device_extensions_count);

    PhysicalDeviceFormatSpecs format_specs;
    {
        switch (s_Context.PhysicalDevice.DepthBufferFormat) {
        case VK_FORMAT_D16_UNORM:          format_specs.DepthBufferFormat = Format::D16_UNORM; break;
        case VK_FORMAT_D32_SFLOAT:         format_specs.DepthBufferFormat = Format::D32_SFLOAT; break;
        case VK_FORMAT_D16_UNORM_S8_UINT:  format_specs.DepthBufferFormat = Format::D16_UNORM_S8_UINT; break;
        case VK_FORMAT_D24_UNORM_S8_UINT:  format_specs.DepthBufferFormat = Format::D24_UNORM_S8_UINT; break;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: format_specs.DepthBufferFormat = Format::D32_SFLOAT_S8_UINT; break;
        default:                           KFATAL("Unsupported depth buffer format %d", s_Context.PhysicalDevice.DepthBufferFormat);
        }
    }

    switch (s_Context.Swapchain.ImageFormat.format) {
    case VK_FORMAT_R8G8B8A8_UNORM: format_specs.SwapchainFormat = Format::RGBA8_UNORM; break;
    case VK_FORMAT_R8G8B8_UNORM:   format_specs.SwapchainFormat = Format::RGB8_UNORM; break;
    case VK_FORMAT_B8G8R8A8_UNORM: format_specs.SwapchainFormat = Format::BGRA8_UNORM; break;
    case VK_FORMAT_B8G8R8_UNORM:   format_specs.SwapchainFormat = Format::BGR8_UNORM; break;
    default:                       KFATAL("Unsupported swapchain image format %d", s_Context.Swapchain.ImageFormat.format);
    }

    s_ResourceManager->SetPhysicalDeviceFormatSpecs(format_specs);

    //
    // Create all rendering resources
    //

    VulkanCreateSwapchain(&s_Context, s_Context.FramebufferWidth, s_Context.FramebufferHeight, s_Context.Options->VSync);

#if !KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    VulkanCreateRenderPass(
        &s_Context, {0.10f, 0.10f, 0.10f, 1.0f}, {0.0f, 0.0f, (float)s_Context.FramebufferWidth, (float)s_Context.FramebufferHeight}, 1.0f, 0, &s_Context.MainRenderPass, true, "MainRenderPass"
    );
    createFramebuffers(&s_Context.Swapchain, &s_Context.MainRenderPass);
#endif
    createCommandBuffers();

    CreateArray(s_Context.ImageAvailableSemaphores, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.RenderCompleteSemaphores, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.InFlightImageToFenceMap, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.WaitFences, s_Context.Swapchain.ImageCount);

    for (u32 i = 0; i < s_Context.Swapchain.ImageCount; ++i) {
        VkSemaphoreCreateInfo create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &create_info, s_Context.AllocationCallbacks, &s_Context.ImageAvailableSemaphores[i]);
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &create_info, s_Context.AllocationCallbacks, &s_Context.RenderCompleteSemaphores[i]);

        VulkanCreateFence(&s_Context, true, &s_Context.WaitFences[i]);
        s_Context.InFlightImageToFenceMap[i] = &s_Context.WaitFences[i];
    }

    s_Context.DefaultTextureSampler = ResourceManager->CreateTextureSampler({});
    KASSERT(s_Context.DefaultTextureSampler.IsInvalid() == false);

    // Global Descriptor Sets
    {
        // First, we create our descriptor pool
        const u32 global_pool_size = 128 * s_Context.Swapchain.ImageCount;
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, global_pool_size},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, global_pool_size},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, global_pool_size},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, global_pool_size},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, global_pool_size},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, global_pool_size},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, global_pool_size},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, global_pool_size},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, global_pool_size},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, global_pool_size},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, global_pool_size},
        };

        VkDescriptorPoolCreateInfo descriptor_pool_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptor_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        descriptor_pool_create_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
        descriptor_pool_create_info.pPoolSizes = &pool_sizes[0];
        descriptor_pool_create_info.maxSets = global_pool_size * sizeof(pool_sizes) / sizeof(pool_sizes[0]);

        vkCreateDescriptorPool(s_Context.LogicalDevice.Handle, &descriptor_pool_create_info, s_Context.AllocationCallbacks, &s_Context.GlobalDescriptorPool);

        // Descriptor sets
        VkDescriptorSetLayoutBinding global_data_layout_bindings[3] = {};

        // Binding #0: Global Shader Data - Projection, View, Camera, etc
        global_data_layout_bindings[0].binding = 0;
        global_data_layout_bindings[0].descriptorCount = 1;
        global_data_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        global_data_layout_bindings[0].pImmutableSamplers = 0;
        global_data_layout_bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

        u32 max_sampler_count = KRAFT_VULKAN_MAX_SAMPLERS_ALLOWED;
        if (s_Context.PhysicalDevice.Properties.limits.maxSamplerAllocationCount < KRAFT_VULKAN_MAX_SAMPLERS_ALLOWED) {
            max_sampler_count = s_Context.PhysicalDevice.Properties.limits.maxSamplerAllocationCount;
            KWARN(
                "Renderer::Init: The device does not support the expected sampler allocation limit of %d (Only %d sampler allocations are supported)",
                KRAFT_VULKAN_MAX_SAMPLERS_ALLOWED,
                max_sampler_count
            );
        }

        // Binding #1: Default texture Sampler
        global_data_layout_bindings[1].binding = 1;
        global_data_layout_bindings[1].descriptorCount = max_sampler_count;
        global_data_layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        global_data_layout_bindings[1].pImmutableSamplers = 0;
        global_data_layout_bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

        // Binding #2: Global materials array
        global_data_layout_bindings[2].binding = 2;
        global_data_layout_bindings[2].descriptorCount = 1;
        global_data_layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        global_data_layout_bindings[2].pImmutableSamplers = 0;
        global_data_layout_bindings[2].stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo global_data_layout_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        global_data_layout_create_info.bindingCount = KRAFT_C_ARRAY_SIZE(global_data_layout_bindings);
        global_data_layout_create_info.pBindings = &global_data_layout_bindings[0];
        global_data_layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &global_data_layout_create_info, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        // Set 1: This Set is unused
        VkDescriptorSetLayoutBinding __unused_set_layout_binding = {};
        __unused_set_layout_binding.binding = 0;
        __unused_set_layout_binding.descriptorCount = 1;
        __unused_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        __unused_set_layout_binding.pImmutableSamplers = 0;
        __unused_set_layout_binding.stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo __unused_set_layout_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        __unused_set_layout_create_info.bindingCount = 1;
        __unused_set_layout_create_info.pBindings = &__unused_set_layout_binding;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &__unused_set_layout_create_info, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        // Set 2: Textures
        // Binding #0: Global texture array
        VkDescriptorSetLayoutBinding texture_data_layout_binding = {};
        texture_data_layout_binding.binding = 0;
        texture_data_layout_binding.descriptorCount = KRAFT_RENDERER__MAX_GLOBAL_TEXTURES; // Number of global textures
        texture_data_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texture_data_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        texture_data_layout_binding.pImmutableSamplers = 0;

        VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        VkDescriptorSetLayoutBindingFlagsCreateInfo set_binding_flags = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        set_binding_flags.bindingCount = 1;
        set_binding_flags.pBindingFlags = &binding_flags;

        VkDescriptorSetLayoutCreateInfo TextureSetLayoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        TextureSetLayoutCreateInfo.bindingCount = 1;
        TextureSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        TextureSetLayoutCreateInfo.pBindings = &texture_data_layout_binding;
        TextureSetLayoutCreateInfo.pNext = &set_binding_flags;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &TextureSetLayoutCreateInfo, s_Context.AllocationCallbacks, &s_Context.DescriptorSetLayouts[s_Context.DescriptorSetLayoutsCount++]
        ));

        u32 texture_count = KRAFT_RENDERER__MAX_GLOBAL_TEXTURES;
        VkDescriptorSetVariableDescriptorCountAllocateInfo set_allocate_count_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO};
        set_allocate_count_info.descriptorSetCount = 1;
        set_allocate_count_info.pDescriptorCounts = &texture_count;

        VkDescriptorSetAllocateInfo set_allocate_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        set_allocate_info.pNext = &set_allocate_count_info;
        set_allocate_info.descriptorPool = s_Context.GlobalDescriptorPool;
        set_allocate_info.descriptorSetCount = 1;
        set_allocate_info.pSetLayouts = &s_Context.DescriptorSetLayouts[2];

        KRAFT_VK_CHECK(vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &set_allocate_info, &s_Context.GlobalTexturesDescriptorSet));
    }

    // Create vertex and index buffers
    if (!createBuffers()) {
        KFATAL("[VulkanRendererBackend::Init]: Failed to create buffers");
        return false;
    }

    KSUCCESS("[VulkanRendererBackend::Init]: Backend init success!");

    return true;
}

bool VulkanRendererBackend::Shutdown() {
    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);
    s_ResourceManager->Clear();

    vkDestroyDescriptorPool(s_Context.LogicalDevice.Handle, s_Context.GlobalDescriptorPool, s_Context.AllocationCallbacks);

    for (u32 i = 0; i < s_Context.DescriptorSetLayoutsCount; i++) {
        vkDestroyDescriptorSetLayout(s_Context.LogicalDevice.Handle, s_Context.DescriptorSetLayouts[i], s_Context.AllocationCallbacks);
    }

    for (u32 i = 0; i < s_Context.Swapchain.ImageCount; ++i) {
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
#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING

#else
    // Destroy framebuffers
    destroyFramebuffers(&s_Context.Swapchain);
    VulkanDestroyRenderPass(&s_Context, &s_Context.MainRenderPass);
#endif

    VulkanDestroySwapchain(&s_Context);
    VulkanDestroyLogicalDevice(&s_Context);

    if (s_Context.Surface) {
        vkDestroySurfaceKHR(s_Context.Instance, s_Context.Surface, s_Context.AllocationCallbacks);
        s_Context.Surface = 0;
    }

    // s_ResourceManager->Destroy();

    return true;
}

i32 VulkanRendererBackend::PrepareFrame() {
    VulkanFence* fence = s_Context.InFlightImageToFenceMap[s_Context.Swapchain.CurrentFrame];
    if (!VulkanWaitForFence(&s_Context, fence, UINT64_MAX)) {
        KERROR("[VulkanRendererBackend::PrepareFrame]: VulkanWaitForFence failed");
        return -1;
    }

    VulkanResetFence(&s_Context, fence);

    // Acquire the next image
    if (!VulkanAcquireNextImageIndex(&s_Context, UINT64_MAX, s_Context.ImageAvailableSemaphores[s_Context.Swapchain.CurrentFrame], 0, &s_Context.CurrentSwapchainImageIndex)) {
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

bool VulkanRendererBackend::BeginFrame() {
    // Record commands
    s_Context.ActiveCommandBuffer = s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];

    VulkanCommandBuffer* gpu_cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanResetCommandBuffer(gpu_cmd_buffer);
    VulkanBeginCommandBuffer(gpu_cmd_buffer, false, false, false);

#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    bool msaa_enabled = s_Context.Swapchain.MSAASampleCount != VK_SAMPLE_COUNT_1_BIT;

    // We must manually transition the swapchain image to the right format for dynamic rendering
    // Depth target
    imageBarrier(
        VulkanResourceManagerApi::GetTexture(s_Context.Swapchain.DepthAttachment)->Image,
        VK_DEPENDENCY_BY_REGION_BIT,
        {
            .src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .src_access_mask = 0,
            .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .base_mip_level = 0,
            .level_count = VK_REMAINING_MIP_LEVELS,
        }
    );

    // Swapchain color target (resolve target when MSAA, render target otherwise)
    imageBarrier(
        s_Context.Swapchain.Images[s_Context.CurrentSwapchainImageIndex],
        VK_DEPENDENCY_BY_REGION_BIT,
        {
            .src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .src_access_mask = 0,
            .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            .base_mip_level = 0,
            .level_count = VK_REMAINING_MIP_LEVELS,
        }
    );

    // MSAA color target barrier
    if (msaa_enabled) {
        imageBarrier(
            VulkanResourceManagerApi::GetTexture(s_Context.Swapchain.MSAAColorAttachment)->Image,
            VK_DEPENDENCY_BY_REGION_BIT,
            {
                .src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .src_access_mask = 0,
                .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                .base_mip_level = 0,
                .level_count = VK_REMAINING_MIP_LEVELS,
            }
        );
    }

    VkRenderingAttachmentInfo color_attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    if (msaa_enabled) {
        // Render to MSAA image, resolve to swapchain
        color_attachment_info.imageView = VulkanResourceManagerApi::GetTexture(s_Context.Swapchain.MSAAColorAttachment)->View;
        color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // MSAA image doesn't need to be stored
        color_attachment_info.clearValue.color = {{0x1a / 255.0f, 0x1a / 255.0f, 0x1a / 255.0f, 1.0f}};
        color_attachment_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        color_attachment_info.resolveImageView = s_Context.Swapchain.ImageViews[s_Context.CurrentSwapchainImageIndex];
        color_attachment_info.resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    } else {
        color_attachment_info.imageView = s_Context.Swapchain.ImageViews[s_Context.CurrentSwapchainImageIndex];
        color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_info.clearValue.color = {{0x1a / 255.0f, 0x1a / 255.0f, 0x1a / 255.0f, 1.0f}};
    }

    VkRenderingAttachmentInfo depth_attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depth_attachment_info.imageView = VulkanResourceManagerApi::GetTexture(s_Context.Swapchain.DepthAttachment)->View;
    depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment_info.clearValue.depthStencil = {1.f, 0};

    VkRenderingInfo rendering_info = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.renderArea.extent.width = s_Context.FramebufferWidth;
    rendering_info.renderArea.extent.height = s_Context.FramebufferHeight;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment_info;
    rendering_info.pDepthAttachment = &depth_attachment_info;

    vkCmdBeginRendering(gpu_cmd_buffer->Resource, &rendering_info);
#else
    VulkanBeginRenderPass(gpu_cmd_buffer, &s_Context.MainRenderPass, s_Context.Swapchain.Framebuffers[s_Context.CurrentSwapchainImageIndex].Handle, VK_SUBPASS_CONTENTS_INLINE);

    s_Context.MainRenderPass.Rect.z = (f32)s_Context.FramebufferWidth;
    s_Context.MainRenderPass.Rect.w = (f32)s_Context.FramebufferHeight;
#endif

    VulkanRendererBackendState.BuffersToSubmit[VulkanRendererBackendState.BuffersToSubmitNum] = gpu_cmd_buffer->Resource;
    VulkanRendererBackendState.BuffersToSubmitNum++;

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = (f32)s_Context.FramebufferHeight;
    viewport.width = (f32)s_Context.FramebufferWidth;
    viewport.height = -(f32)s_Context.FramebufferHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(gpu_cmd_buffer->Resource, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = {s_Context.FramebufferWidth, s_Context.FramebufferHeight};

    vkCmdSetScissor(gpu_cmd_buffer->Resource, 0, 1, &scissor);
    vkCmdSetCullMode(gpu_cmd_buffer->Resource, VK_CULL_MODE_NONE);
    // vkCmdSetDepthBias(gpu_cmd_buffer->Resource, 16, 0, 1);

    return true;
}

bool VulkanRendererBackend::EndFrame() {
    VulkanCommandBuffer* gpu_cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);

#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    vkCmdEndRendering(gpu_cmd_buffer->Resource);

    // Transition the image to be present-able
    imageBarrier(
        s_Context.Swapchain.Images[s_Context.CurrentSwapchainImageIndex],
        VK_DEPENDENCY_BY_REGION_BIT,
        {
            .src_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dst_access_mask = 0,
            .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
            .base_mip_level = 0,
            .level_count = VK_REMAINING_MIP_LEVELS,
        }
    );
#else
    VulkanEndRenderPass(gpu_cmd_buffer, &s_Context.MainRenderPass);
#endif

    VulkanEndCommandBuffer(gpu_cmd_buffer);

    VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = VulkanRendererBackendState.BuffersToSubmitNum;
    submit_info.pCommandBuffers = &VulkanRendererBackendState.BuffersToSubmit[0];
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &s_Context.ImageAvailableSemaphores[s_Context.Swapchain.CurrentFrame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &s_Context.RenderCompleteSemaphores[s_Context.CurrentSwapchainImageIndex];
    submit_info.pWaitDstStageMask = &wait_stage_mask;

    KRAFT_VK_CHECK(vkQueueSubmit(s_Context.LogicalDevice.GraphicsQueue, 1, &submit_info, s_Context.InFlightImageToFenceMap[s_Context.Swapchain.CurrentFrame]->Handle));

    VulkanSetCommandBufferSubmitted(gpu_cmd_buffer);

    // Present the swapchain!
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &s_Context.RenderCompleteSemaphores[s_Context.CurrentSwapchainImageIndex];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &s_Context.Swapchain.Resource;
    present_info.pImageIndices = &s_Context.CurrentSwapchainImageIndex;
    present_info.pResults = 0;

    VkResult result = vkQueuePresentKHR(s_Context.LogicalDevice.PresentQueue, &present_info);
    // Common case
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        s_Context.Swapchain.CurrentFrame = (s_Context.Swapchain.CurrentFrame + 1) % s_Context.Swapchain.MaxFramesInFlight;
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        VulkanRecreateSwapchain(&s_Context);
    } else {
        KERROR("[VulkanPresentSwapchain]: Presentation failed!");
    }

    VulkanRendererBackendState.BuffersToSubmitNum = 0;

    return true;
}

void VulkanRendererBackend::OnResize(int width, int height) {
    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);

    if (width == 0 || height == 0) {
        KERROR("Cannot recreate swapchain because width/height is 0 | %d, %d", width, height);
        return;
    }

    s_Context.FramebufferWidth = width;
    s_Context.FramebufferHeight = height;

    VulkanRecreateSwapchain(&s_Context);

    destroyCommandBuffers();
    createCommandBuffers();

#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING

#else
    s_Context.MainRenderPass.Rect = {0.0f, 0.0f, (float)s_Context.FramebufferWidth, (float)s_Context.FramebufferHeight};
    createFramebuffers(&s_Context.Swapchain, &s_Context.MainRenderPass);
#endif
}

void VulkanRendererBackend::CreateRenderPipeline(Shader* shader) {
    TempArena scratch = ScratchBegin(0, 0);
    const shaderfx::ShaderEffect& effect = shader->ShaderEffect;

    // === SHARED: Descriptor set layouts and pipeline layout (same for all variants) ===

    VkDescriptorSetLayout descriptor_set_layouts[KRAFT_VULKAN_MAX_DESCRIPTOR_SETS_PER_SHADER] = {0};
    u32 layouts_count = 0;
    while (layouts_count < s_Context.DescriptorSetLayoutsCount) {
        descriptor_set_layouts[layouts_count++] = s_Context.DescriptorSetLayouts[layouts_count];
    }

    KASSERT(layouts_count <= KRAFT_VULKAN_NUM_INBUILT_DESCRIPTOR_SETS);

    VulkanShader* internal_shader_data = (VulkanShader*)Malloc(sizeof(VulkanShader), MEMORY_TAG_RENDERER, true);

    // Collect bindings into sets
    shaderfx::ResourceBinding bindings_by_set[KRAFT_VULKAN_MAX_DESCRIPTOR_SETS_PER_SHADER][KRAFT_VULKAN_MAX_BINDINGS_PER_SET] = {};
    u32 max_set_number = 0;
    for (u32 i = 0; i < effect.global_resource_count; i++) {
        for (u32 j = 0; j < effect.global_resources[i].binding_count; j++) {
            const shaderfx::ResourceBinding& binding = effect.global_resources[i].bindings[j];
            bindings_by_set[binding.set][binding.binding] = binding;

            max_set_number = binding.set > max_set_number ? binding.set : max_set_number;
        }
    }

    for (u32 i = 0; i < effect.local_resource_count; i++) {
        for (u32 j = 0; j < effect.local_resources[i].binding_count; j++) {
            const shaderfx::ResourceBinding& binding = effect.local_resources[i].bindings[j];
            bindings_by_set[binding.set][binding.binding] = binding;

            max_set_number = binding.set > max_set_number ? binding.set : max_set_number;
        }
    }

    KASSERT(max_set_number < KRAFT_VULKAN_MAX_DESCRIPTOR_SETS_PER_SHADER);

    for (u32 set = KRAFT_VULKAN_NUM_INBUILT_DESCRIPTOR_SETS; set <= max_set_number; set++) {
        VkDescriptorSetLayoutBinding* bindings = ArenaPushArray(scratch.arena, VkDescriptorSetLayoutBinding, KRAFT_VULKAN_MAX_BINDINGS);
        u32 binding_count = 0;
        for (u32 binding_idx = 0; binding_idx < 16; binding_idx++) {
            const shaderfx::ResourceBinding& binding = bindings_by_set[set][binding_idx];
            if (binding.set == set) {
                VkDescriptorSetLayoutBinding* binding_ref = &bindings[binding_count];
                binding_ref->binding = binding.binding;
                binding_ref->descriptorCount = 1;
                binding_ref->descriptorType = ToVulkanResourceType(binding.type);
                binding_ref->pImmutableSamplers = 0;
                binding_ref->stageFlags = ToVulkanShaderStageFlags(binding.stage);
                binding_count++;
            }
        }

        VkDescriptorSetLayoutCreateInfo descriptor_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptor_create_info.bindingCount = binding_count;
        descriptor_create_info.pBindings = &bindings[0];

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, &descriptor_create_info, s_Context.AllocationCallbacks, &internal_shader_data->descriptor_set_layouts[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)]
        ));

        VkDescriptorSetAllocateInfo set_allocate_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        set_allocate_info.descriptorPool = s_Context.GlobalDescriptorPool;
        set_allocate_info.descriptorSetCount = 1;
        set_allocate_info.pSetLayouts = &internal_shader_data->descriptor_set_layouts[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)];

        vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &set_allocate_info, &internal_shader_data->descriptor_sets[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)]);

        descriptor_set_layouts[set] = internal_shader_data->descriptor_set_layouts[set - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)];
        layouts_count = set + 1;
    }

    VkPipelineLayoutCreateInfo layout_create_info = {};
    layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_create_info.setLayoutCount = layouts_count;
    layout_create_info.pSetLayouts = &descriptor_set_layouts[0];
    layout_create_info.pushConstantRangeCount = 1;
    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = 128,
    };
    layout_create_info.pPushConstantRanges = &push_constant_range;

    VkPipelineLayout pipeline_layout;
    KRAFT_VK_CHECK(vkCreatePipelineLayout(s_Context.LogicalDevice.Handle, &layout_create_info, s_Context.AllocationCallbacks, &pipeline_layout));
    KASSERT(pipeline_layout);

    String8 shader_effect_name = effect.name;
    String8 debug_pipeline_layout_name = StringCat(scratch.arena, shader_effect_name, String8Raw("PipelineLayout"));

    KRAFT_RENDERER_SET_OBJECT_NAME(pipeline_layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, debug_pipeline_layout_name.str);

    internal_shader_data->PipelineLayout = pipeline_layout;
    internal_shader_data->PipelineCount = effect.variant_count;
    internal_shader_data->Pipelines = (VkPipeline*)Malloc(sizeof(VkPipeline) * effect.variant_count, MEMORY_TAG_RENDERER, true);

    // Create a pipeline for every shader variant specified
    for (u32 v = 0; v < effect.variant_count; v++) {
        const shaderfx::VariantDefinition& variant = effect.variants[v];
        VkGraphicsPipelineCreateInfo pipeline_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

        // Shader stages
        u32 shader_stage_count = variant.shader_stage_count;
        VkPipelineShaderStageCreateInfo* pipeline_shader_stage_create_infos = ArenaPushArray(scratch.arena, VkPipelineShaderStageCreateInfo, shader_stage_count);
        VkShaderModule* shader_modules = ArenaPushArray(scratch.arena, VkShaderModule, shader_stage_count);

        for (int j = 0; j < shader_stage_count; j++) {
            const shaderfx::VariantDefinition::ShaderDefinition& shader_def = variant.shader_stages[j];

            VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.codeSize = shader_def.code_fragment.code.count;
            info.pCode = (u32*)shader_def.code_fragment.code.ptr;

            VkShaderModule shader_module;
            KRAFT_VK_CHECK(vkCreateShaderModule(s_Context.LogicalDevice.Handle, &info, s_Context.AllocationCallbacks, &shader_module));
            KASSERT(shader_module);

            VkPipelineShaderStageCreateInfo shader_stage_info = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            shader_stage_info.module = shader_module;
            shader_stage_info.pName = "main";
            shader_stage_info.stage = ToVulkanShaderStageFlagBits(shader_def.stage);

            pipeline_shader_stage_create_infos[j] = shader_stage_info;
            shader_modules[j] = shader_module;
        }

        // Input bindings and attributes
        // u64                              input_binding_count = variant.vertex_layout->input_binding_count;
        // VkVertexInputBindingDescription* input_binding_descs = ArenaPushArray(scratch.arena, VkVertexInputBindingDescription, input_binding_count);
        // for (int j = 0; j < input_binding_count; j++)
        // {
        //     const shaderfx::VertexInputBinding& input_binding_def = variant.vertex_layout->input_bindings[j];
        //     input_binding_descs[j].binding = input_binding_def.binding;
        //     input_binding_descs[j].inputRate = input_binding_def.input_rate == VertexInputRate::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        //     input_binding_descs[j].stride = input_binding_def.stride;
        // }

        // u64                                attribute_count = variant.vertex_layout->attribute_count;
        // VkVertexInputAttributeDescription* attribute_descs = ArenaPushArray(scratch.arena, VkVertexInputAttributeDescription, attribute_count);
        // for (int j = 0; j < attribute_count; j++)
        // {
        //     const shaderfx::VertexAttribute& vertex_attribute_def = variant.vertex_layout->attributes[j];
        //     attribute_descs[j].location = vertex_attribute_def.location;
        //     attribute_descs[j].format = ToVulkanFormat(vertex_attribute_def.format);
        //     attribute_descs[j].binding = vertex_attribute_def.binding;
        //     attribute_descs[j].offset = vertex_attribute_def.offset;
        // }

        // "Hello world"
        // [  ]

        VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
        viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_create_info.viewportCount = 1;
        viewport_state_create_info.scissorCount = 1;
        pipeline_create_info.pViewportState = &viewport_state_create_info;

        VkPipelineRasterizationStateCreateInfo raster_state_create_info = {};
        raster_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster_state_create_info.depthClampEnable = VK_FALSE;
        raster_state_create_info.rasterizerDiscardEnable = VK_FALSE;
        raster_state_create_info.polygonMode = ToVulkanPolygonMode(variant.render_state->polygon_mode);
        raster_state_create_info.lineWidth = s_Context.PhysicalDevice.Features.wideLines ? variant.render_state->line_width : 1.0f;
        raster_state_create_info.cullMode = ToVulkanCullModeFlagBits(variant.render_state->cull_mode);
        raster_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster_state_create_info.depthBiasEnable = VK_FALSE;
        raster_state_create_info.depthBiasConstantFactor = 0.0f;
        raster_state_create_info.depthBiasClamp = 0.0f;
        raster_state_create_info.depthBiasSlopeFactor = 0.0f;
        pipeline_create_info.pRasterizationState = &raster_state_create_info;

        VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
        multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_create_info.sampleShadingEnable = VK_FALSE;
        multisample_state_create_info.rasterizationSamples = variant.msaa_samples > 0 ? (VkSampleCountFlagBits)variant.msaa_samples : s_Context.Swapchain.MSAASampleCount;
        multisample_state_create_info.minSampleShading = 1.0f;
        multisample_state_create_info.pSampleMask = 0;
        multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
        multisample_state_create_info.alphaToOneEnable = VK_FALSE;
        pipeline_create_info.pMultisampleState = &multisample_state_create_info;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
        depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_create_info.depthTestEnable = variant.render_state->z_test_op > CompareOp::Never ? VK_TRUE : VK_FALSE;
        depth_stencil_state_create_info.depthWriteEnable = variant.render_state->z_write_enable;
        depth_stencil_state_create_info.depthCompareOp = ToVulkanCompareOp(variant.render_state->z_test_op);
        depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
        pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;

        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;

        if (variant.has_color_output) {
            color_blend_attachment_state.blendEnable = variant.render_state->blend_enable;
            if (variant.render_state->blend_enable) {
                color_blend_attachment_state.srcColorBlendFactor = ToVulkanBlendFactor(variant.render_state->blend_mode.src_color_blend_factor);
                color_blend_attachment_state.dstColorBlendFactor = ToVulkanBlendFactor(variant.render_state->blend_mode.dst_color_blend_factor);
                color_blend_attachment_state.colorBlendOp = ToVulkanBlendOp(variant.render_state->blend_mode.color_blend_op);
                color_blend_attachment_state.srcAlphaBlendFactor = ToVulkanBlendFactor(variant.render_state->blend_mode.src_alpha_blend_factor);
                color_blend_attachment_state.dstAlphaBlendFactor = ToVulkanBlendFactor(variant.render_state->blend_mode.dst_alpha_blend_factor);
                color_blend_attachment_state.alphaBlendOp = ToVulkanBlendOp(variant.render_state->blend_mode.alpha_blend_op);
            }
            color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            color_blend_state_create_info.attachmentCount = 1;
            color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
        } else {
            color_blend_state_create_info.attachmentCount = 0;
            color_blend_state_create_info.pAttachments = nullptr;
        }

        pipeline_create_info.pColorBlendState = &color_blend_state_create_info;

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
        dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_create_info.pDynamicStates = dynamic_states;
        dynamic_state_create_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
        pipeline_create_info.pDynamicState = &dynamic_state_create_info;

        // Vertices are now fetched from a storage buffer
        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;

        pipeline_create_info.layout = pipeline_layout;
        pipeline_create_info.stageCount = (u32)shader_stage_count;
        pipeline_create_info.pStages = &pipeline_shader_stage_create_infos[0];
        pipeline_create_info.pTessellationState = 0;

#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
        const VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;

        VkPipelineRenderingCreateInfo rendering_create_info = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rendering_create_info.colorAttachmentCount = variant.has_color_output ? 1 : 0;
        rendering_create_info.pColorAttachmentFormats = variant.has_color_output ? &color_format : nullptr;
        rendering_create_info.depthAttachmentFormat = variant.has_depth_output ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED;
        pipeline_create_info.pNext = &rendering_create_info;
        pipeline_create_info.renderPass = NULL;
#else
        pipeline_create_info.renderPass = s_Context.MainRenderPass.Handle;
#endif

        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.basePipelineIndex = -1;

        VkPipeline pipeline;
        KRAFT_VK_CHECK(vkCreateGraphicsPipelines(s_Context.LogicalDevice.Handle, VK_NULL_HANDLE, 1, &pipeline_create_info, s_Context.AllocationCallbacks, &pipeline));
        KASSERT(pipeline);

        String8 debug_pipeline_name = StringCat(scratch.arena, shader_effect_name, String8Raw("_"));
        debug_pipeline_name = StringCat(scratch.arena, debug_pipeline_name, variant.name);
        debug_pipeline_name = StringCat(scratch.arena, debug_pipeline_name, String8Raw("Pipeline"));
        KRAFT_RENDERER_SET_OBJECT_NAME(pipeline, VK_OBJECT_TYPE_PIPELINE, debug_pipeline_name.str);

        internal_shader_data->Pipelines[v] = pipeline;

        // Release shader modules for this variant
        for (int j = 0; j < shader_stage_count; j++) {
            vkDestroyShaderModule(s_Context.LogicalDevice.Handle, shader_modules[j], s_Context.AllocationCallbacks);
        }
    }

    shader->RendererData = internal_shader_data;

    ScratchEnd(scratch);
}

void VulkanRendererBackend::DestroyRenderPipeline(Shader* Shader) {
    if (Shader->RendererData) {
        vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);
        VulkanShader* VulkanShaderData = (VulkanShader*)Shader->RendererData;

        for (u32 i = 0; i < VulkanShaderData->PipelineCount; i++) {
            vkDestroyPipeline(s_Context.LogicalDevice.Handle, VulkanShaderData->Pipelines[i], s_Context.AllocationCallbacks);
        }

        vkDestroyPipelineLayout(s_Context.LogicalDevice.Handle, VulkanShaderData->PipelineLayout, s_Context.AllocationCallbacks);

        Free(VulkanShaderData->Pipelines, sizeof(VkPipeline) * VulkanShaderData->PipelineCount, MEMORY_TAG_RENDERER);
        Free(Shader->RendererData, sizeof(VulkanShader), MEMORY_TAG_RENDERER);
        Shader->RendererData = 0;
    }
}

void VulkanRendererBackend::UseShader(const Shader* Shader, u32 variant_index) {
    VulkanShader* VulkanShaderData = (VulkanShader*)Shader->RendererData;
    KASSERT(variant_index < VulkanShaderData->PipelineCount);
    VkPipeline Pipeline = VulkanShaderData->Pipelines[variant_index];
    VulkanCommandBuffer* GPUCmdBuffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);

    vkCmdBindPipeline(GPUCmdBuffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
}

void VulkanRendererBackend::ApplyGlobalShaderProperties(Shader* shader, Handle<Buffer> ubo_buffer, Handle<Buffer> materials_buffer) {
    VulkanCommandBuffer* cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanShader* shader_data = (VulkanShader*)shader->RendererData;

    VkBuffer gpu_buffer = VulkanResourceManagerApi::GetBuffer(ubo_buffer)->Handle;
    VkDescriptorBufferInfo global_data_buffer_info = {};
    global_data_buffer_info.buffer = gpu_buffer;
    global_data_buffer_info.offset = 0;
    global_data_buffer_info.range = VK_WHOLE_SIZE;

    u32 count = 0;
    VkWriteDescriptorSet descriptor_write_info[3] = {};
    descriptor_write_info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write_info[0].descriptorCount = 1;
    descriptor_write_info[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write_info[0].dstBinding = 0;
    descriptor_write_info[0].dstArrayElement = 0;
    descriptor_write_info[0].pBufferInfo = &global_data_buffer_info;
    count++;

    // Collect all active samplers into an array for binding
    VkSampler active_samplers[KRAFT_VULKAN_MAX_SAMPLERS_ALLOWED];
    u32 sampler_count = VulkanResourceManagerApi::GetActiveSamplers(active_samplers, KRAFT_VULKAN_MAX_SAMPLERS_ALLOWED);

    VkDescriptorImageInfo sampler_image_infos[KRAFT_VULKAN_MAX_SAMPLERS_ALLOWED];
    for (u32 i = 0; i < sampler_count; i++) {
        sampler_image_infos[i].sampler = active_samplers[i];
        sampler_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        sampler_image_infos[i].imageView = VK_NULL_HANDLE;
    }

    descriptor_write_info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write_info[1].descriptorCount = sampler_count;
    descriptor_write_info[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptor_write_info[1].dstBinding = 1;
    descriptor_write_info[1].dstArrayElement = 0;
    descriptor_write_info[1].pImageInfo = sampler_image_infos;
    count++;

    VulkanBuffer* materials_gpu_buffer = VulkanResourceManagerApi::GetBuffer(materials_buffer);
    VkDescriptorBufferInfo materials_buffer_info = {};
    if (materials_gpu_buffer) {
        materials_buffer_info.buffer = materials_gpu_buffer->Handle;
        materials_buffer_info.range = VK_WHOLE_SIZE;

        descriptor_write_info[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write_info[2].descriptorCount = 1;
        descriptor_write_info[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor_write_info[2].dstBinding = 2;
        descriptor_write_info[2].dstArrayElement = 0;
        descriptor_write_info[2].pBufferInfo = &materials_buffer_info;
        count++;
    }

    vkCmdPushDescriptorSetKHR(cmd_buffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, shader_data->PipelineLayout, 0, count, &descriptor_write_info[0]);
    vkCmdBindDescriptorSets(cmd_buffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, shader_data->PipelineLayout, 2, 1, &s_Context.GlobalTexturesDescriptorSet, 0, nullptr);
}

void VulkanRendererBackend::ApplyLocalShaderProperties(Shader* shader, void* data) {
    VulkanCommandBuffer* cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanShader* shader_data = (VulkanShader*)shader->RendererData;

    vkCmdPushConstants(cmd_buffer->Resource, shader_data->PipelineLayout, VK_SHADER_STAGE_ALL, 0, 128, data);
}

void VulkanRendererBackend::UpdateTextures(Handle<Texture>* textures, u64 texture_count) {
    for (u32 i = 0; i < texture_count; i++) {
        VulkanTexture* texture = VulkanResourceManagerApi::GetTexture(textures[i]);
        VkDescriptorImageInfo image_info = {};
        image_info.imageView = texture->View;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write_descriptor_set = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write_descriptor_set.dstSet = s_Context.GlobalTexturesDescriptorSet;
        write_descriptor_set.dstBinding = 0;
        write_descriptor_set.dstArrayElement = textures[i].GetIndex();
        write_descriptor_set.descriptorCount = 1;
        write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write_descriptor_set.pImageInfo = &image_info;

        vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, 1, &write_descriptor_set, 0, nullptr);
    }
}

void VulkanRendererBackend::DrawGeometryData(GeometryDrawData draw_data, Handle<Buffer> index_buffer_handle) {
    VulkanCommandBuffer* cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanBuffer* index_buffer = VulkanResourceManagerApi::GetBuffer(index_buffer_handle);

    vkCmdBindIndexBuffer(cmd_buffer->Resource, index_buffer->Handle, draw_data.IndexBufferOffset, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd_buffer->Resource, draw_data.IndexCount, 1, 0, 0, 0);
}

static bool UploadDataToGPU(VulkanContext* context, Handle<Buffer> dst_buffer, u32 dst_buffer_offset, const void* data, u32 size) {
    BufferView staging_buffer = ResourceManager->CreateTempBuffer(size);
    MemCpy(staging_buffer.Ptr, data, size);

    ResourceManager->UploadBuffer({
        .DstBuffer = dst_buffer,
        .SrcBuffer = staging_buffer.GPUBuffer,
        .SrcSize = size,
        .DstOffset = dst_buffer_offset,
        .SrcOffset = staging_buffer.Offset,
    });

    return true;
}

bool VulkanRendererBackend::CreateGeometry(const GeometryDescription& description) {
    if (description.Vertices && description.VertexCount > 0) {
        u32 vertex_data_size = description.VertexSize * description.VertexCount;
        UploadDataToGPU(&s_Context, description.VertexBuffer, description.VertexBufferOffset, description.Vertices, vertex_data_size);
    }

    if (description.Indices && description.IndexCount > 0) {
        u32 index_data_size = description.IndexSize * description.IndexCount;
        UploadDataToGPU(&s_Context, description.IndexBuffer, description.IndexBufferOffset, description.Indices, index_data_size);
    }

    return true;
}

bool VulkanRendererBackend::UpdateGeometry(const GeometryDescription& description) {
    u32 vertex_data_size = description.VertexSize * description.VertexCount;
    UploadDataToGPU(&s_Context, description.VertexBuffer, description.VertexBufferOffset, description.Vertices, vertex_data_size);

    if (description.Indices && description.IndexCount > 0) {
        u32 index_data_size = description.IndexSize * description.IndexCount;
        UploadDataToGPU(&s_Context, description.IndexBuffer, description.IndexBufferOffset, description.Indices, index_data_size);
    }

    return true;
}

void VulkanRendererBackend::BeginSurface(RenderSurface* surface) {
    Handle<CommandBuffer> cmd_buffer_handle = surface->cmd_buffers[s_Context.CurrentSwapchainImageIndex];
    s_Context.ActiveCommandBuffer = cmd_buffer_handle;
    VulkanCommandBuffer* gpu_cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(cmd_buffer_handle);
    VulkanResetCommandBuffer(gpu_cmd_buffer);
    VulkanBeginCommandBuffer(gpu_cmd_buffer, false, false, false);

    // Transition the attachments to the right format
    // Before rendering, they must be in VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
    if (surface->depth_pass) {
        imageBarrier(
            VulkanResourceManagerApi::GetTexture(surface->depth_pass)->Image,
            VK_DEPENDENCY_BY_REGION_BIT,
            {
                .src_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dst_stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .src_access_mask = 0,
                .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .base_mip_level = 0,
                .level_count = VK_REMAINING_MIP_LEVELS,
            }
        );
    }

    // Gbuffer target
    if (surface->color_pass) {
        imageBarrier(
            VulkanResourceManagerApi::GetTexture(surface->color_pass)->Image,
            VK_DEPENDENCY_BY_REGION_BIT,
            {
                .src_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .src_access_mask = 0,
                .dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                .base_mip_level = 0,
                .level_count = VK_REMAINING_MIP_LEVELS,
            }
        );
    }

#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    VkRenderingInfo rendering_info = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.renderArea.extent.width = s_Context.FramebufferWidth;
    rendering_info.renderArea.extent.height = s_Context.FramebufferHeight;
    rendering_info.layerCount = 1;

    VkRenderingAttachmentInfo color_attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    if (surface->color_pass) {
        VkImageView color_pass_view = VulkanResourceManagerApi::GetTexture(surface->color_pass)->View;
        color_attachment_info.imageView = color_pass_view;
        color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // color_attachment_info.clearValue.color =  {{0x1a / 255.0f, 0x1a / 255.0f, 0x1a / 255.0f, 1.0f}};
        color_attachment_info.clearValue.color = {{0x1a / 255.0f, 0x1a / 255.0f, 0x1a / 255.0f, 1.0f}};

        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment_info;
    }

    VkRenderingAttachmentInfo depth_attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    if (surface->depth_pass) {
        VkImageView depth_pass_view = VulkanResourceManagerApi::GetTexture(surface->depth_pass)->View;
        depth_attachment_info.imageView = depth_pass_view;
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment_info.clearValue.depthStencil = {1.f, 0};
        rendering_info.pDepthAttachment = &depth_attachment_info;
    }

    vkCmdBeginRendering(gpu_cmd_buffer->Resource, &rendering_info);
#else
    RenderPass* render_pass = s_ResourceManager->GetRenderPassMetadata(render_pass_handle);
    VulkanRenderPass* gpu_render_pass = VulkanResourceManagerApi::GetRenderPass(render_pass);
    VkRenderPassBeginInfo render_pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_begin_info.framebuffer = gpu_render_pass->Framebuffer.Handle;
    render_pass_begin_info.renderPass = gpu_render_pass->Handle;
    render_pass_begin_info.renderArea.offset.x = 0;
    render_pass_begin_info.renderArea.offset.y = 0;
    render_pass_begin_info.renderArea.extent.width = (u32)render_pass->Dimensions.x;
    render_pass_begin_info.renderArea.extent.height = (u32)render_pass->Dimensions.y;

    VkClearValue clear_values[32] = {};
    u32 color_values_index = 0;
    for (int i = 0; i < render_pass->ColorTargets.Size(); i++) {
        ColorTarget target = render_pass->ColorTargets[i];
        clear_values[i].color.f32[0] = target.ClearColor.r;
        clear_values[i].color.f32[1] = target.ClearColor.g;
        clear_values[i].color.f32[2] = target.ClearColor.b;
        clear_values[i].color.f32[3] = target.ClearColor.a;

        color_values_index++;
    }

    clear_values[color_values_index].depthStencil.depth = render_pass->DepthTarget.Depth;
    clear_values[color_values_index].depthStencil.stencil = render_pass->DepthTarget.Stencil;
    color_values_index++;

    render_pass_begin_info.clearValueCount = color_values_index;
    render_pass_begin_info.pClearValues = clear_values;

    // TODO: Figure out how to handle "Contents" parameter
    vkCmdBeginRenderPass(gpu_render_pass->Resource, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
#endif
    gpu_cmd_buffer->State = VULKAN_COMMAND_BUFFER_STATE_IN_RENDER_PASS;

    VkViewport viewport = {};
    viewport.width = (f32)surface->width;
    viewport.height = (f32)surface->height;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(gpu_cmd_buffer->Resource, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = {surface->width, surface->height};

    vkCmdSetScissor(gpu_cmd_buffer->Resource, 0, 1, &scissor);

    VulkanRendererBackendState.BuffersToSubmit[VulkanRendererBackendState.BuffersToSubmitNum] = gpu_cmd_buffer->Resource;
    VulkanRendererBackendState.BuffersToSubmitNum++;
}

void VulkanRendererBackend::EndSurface(RenderSurface* surface) {
    Handle<CommandBuffer> cmd_buffer_handle = surface->cmd_buffers[s_Context.CurrentSwapchainImageIndex];
    VulkanCommandBuffer* gpu_cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(cmd_buffer_handle);

#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    vkCmdEndRendering(gpu_cmd_buffer->Resource);
#else
    vkCmdEndRenderPass(gpu_cmd_buffer->Resource);
#endif

    // VkMemoryBarrier2 MemoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    // MemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // MemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    // MemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_HOST_BIT;
    // MemoryBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    // VkDependencyInfo DependencyInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    // DependencyInfo.memoryBarrierCount = 1;
    // DependencyInfo.pMemoryBarriers = &MemoryBarrier;

    // vkCmdPipelineBarrier2(GPUCmdBuffer->Resource, &DependencyInfo);

    // Now we must transition the surface attachments to the format the user expects them to be in
    if (surface->depth_pass) {
        imageBarrier(
            VulkanResourceManagerApi::GetTexture(surface->depth_pass)->Image,
            VK_DEPENDENCY_BY_REGION_BIT,
            {
                .src_stage_mask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .src_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
                .old_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .base_mip_level = 0,
                .level_count = VK_REMAINING_MIP_LEVELS,
            }
        );
    }

    // Gbuffer target
    if (surface->color_pass) {
        imageBarrier(
            VulkanResourceManagerApi::GetTexture(surface->color_pass)->Image,
            VK_DEPENDENCY_BY_REGION_BIT,
            {
                .src_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .src_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
                .old_layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
                .base_mip_level = 0,
                .level_count = VK_REMAINING_MIP_LEVELS,
            }
        );
    }

    VulkanEndCommandBuffer(gpu_cmd_buffer);
    VulkanSetCommandBufferSubmitted(gpu_cmd_buffer);
}

void VulkanRendererBackend::CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx) {
    VulkanCommandBuffer* cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanShader* shader_data = (VulkanShader*)shader->RendererData;

    VkBuffer buffer_handle = VulkanResourceManagerApi::GetBuffer(buffer)->Handle;
    VkDescriptorBufferInfo info = {};
    info.buffer = buffer_handle;
    info.offset = 0;
    info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write_info = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write_info.descriptorCount = 1;
    write_info.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_info.dstBinding = binding_idx;
    write_info.pBufferInfo = &info;
    write_info.dstSet = shader_data->descriptor_sets[set_idx - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)];

    vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, 1, &write_info, 0, 0);
    vkCmdBindDescriptorSets(
        cmd_buffer->Resource,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shader_data->PipelineLayout,
        set_idx,
        1,
        &shader_data->descriptor_sets[set_idx - (KRAFT_VULKAN_NUM_CUSTOM_DESCRIPTOR_SETS - 1)],
        0,
        nullptr
    );
}

void VulkanRendererBackend::BindBindGroup(Shader* shader, Handle<BindGroup> bind_group) {
    VulkanCommandBuffer* cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VulkanShader* shader_data = (VulkanShader*)shader->RendererData;
    VulkanBindGroup* vk_bind_group = VulkanResourceManagerApi::GetBindGroup(bind_group);
    BindGroup* metadata = VulkanResourceManagerApi::GetBindGroupMetadata(bind_group);

    vkCmdBindDescriptorSets(cmd_buffer->Resource, VK_PIPELINE_BIND_POINT_GRAPHICS, shader_data->PipelineLayout, metadata->set_index, 1, &vk_bind_group->set, 0, nullptr);
}

static void imageBarrier(VkImage image, VkDependencyFlags dependency_flags, VulkanImageBarrierDescription description) {
    VulkanCommandBuffer* cmd_buffer = VulkanResourceManagerApi::GetCommandBuffer(s_Context.ActiveCommandBuffer);
    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};

    barrier.srcStageMask = description.src_stage_mask;
    barrier.dstStageMask = description.dst_stage_mask;
    barrier.srcAccessMask = description.src_access_mask;
    barrier.dstAccessMask = description.dst_access_mask;
    barrier.oldLayout = description.old_layout;
    barrier.newLayout = description.new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = description.aspect_mask;
    barrier.subresourceRange.baseMipLevel = description.base_mip_level;
    barrier.subresourceRange.levelCount = description.level_count;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dependency_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency_info.dependencyFlags = dependency_flags;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd_buffer->Resource, &dependency_info);
}

// void VulkanRendererBackend::ImageBarrier(Handle<Texture> texture, VkDependencyFlags dependency_flags, VulkanImageBarrierDescription description)
// {
//     VkImage image = VulkanResourceManagerApi::GetTexture(texture)->Image;
//     imageBarrier(image, dependency_flags, description);
// }

#ifdef KRAFT_RENDERER_DEBUG
static VkBool32
DebugUtilsMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
        printf("%s\n", pCallbackData->pMessage);
        fflush(stdout);
    } break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
        KWARN("%s", pCallbackData->pMessage);
    } break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
        KDEBUG("%s", pCallbackData->pMessage);
    } break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
        KDEBUG("%s", pCallbackData->pMessage);
    } break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: break;
    }

    return VK_FALSE;
}

static void SetObjectName(u64 Object, VkObjectType ObjectType, const char* Name) {
    VkDebugUtilsObjectNameInfoEXT NameInfo = {};
    NameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    NameInfo.objectType = ObjectType;
    NameInfo.objectHandle = Object;
    NameInfo.pObjectName = Name;
    vkSetDebugUtilsObjectNameEXT(s_Context.LogicalDevice.Handle, &NameInfo);
}
#endif

bool createBuffers() {
    return true;
}

void createCommandBuffers() {
    for (u32 i = 0; i < s_Context.Swapchain.ImageCount; ++i) {
        s_Context.GraphicsCommandBuffers[i] = s_ResourceManager->CreateCommandBuffer({
            .DebugName = "MainCmdBuffer",
            .CommandPool = s_Context.GraphicsCommandPool,
            .Primary = true,
        });
    }

    KDEBUG("[VulkanRendererBackend::Init]: Graphics command buffers created");
}

void destroyCommandBuffers() {
    for (u32 i = 0; i < s_Context.Swapchain.ImageCount; ++i) {
        if (s_Context.GraphicsCommandBuffers[i]) {
            s_ResourceManager->DestroyCommandBuffer(s_Context.GraphicsCommandBuffers[i]);
        }
    }
}

#if !KRAFT_ENABLE_VK_DYNAMIC_RENDERING
void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* render_pass) {
    if (!swapchain->Framebuffers) {
        arrsetlen(swapchain->Framebuffers, swapchain->ImageCount);
        MemZero(swapchain->Framebuffers, sizeof(VulkanFramebuffer) * swapchain->ImageCount);
    }

    VkImageView attachments[] = {0, VulkanResourceManagerApi::GetTexture(swapchain->DepthAttachment)->View};
    for (u32 i = 0; i < swapchain->ImageCount; ++i) {
        if (swapchain->Framebuffers[i].Handle) {
            VulkanDestroyFramebuffer(&s_Context, &swapchain->Framebuffers[i]);
        }

        attachments[0] = swapchain->ImageViews[i];
        VulkanCreateFramebuffer(&s_Context, s_Context.FramebufferWidth, s_Context.FramebufferHeight, render_pass, attachments, KRAFT_C_ARRAY_SIZE(attachments), &swapchain->Framebuffers[i]);
    }
}

void destroyFramebuffers(VulkanSwapchain* swapchain) {
    if (swapchain->Framebuffers) {
        for (int i = 0; i < arrlen(swapchain->Framebuffers); ++i) {
            VulkanDestroyFramebuffer(&s_Context, &swapchain->Framebuffers[i]);
        }
    }

    arrfree(swapchain->Framebuffers);
}
#endif

} // namespace kraft::r