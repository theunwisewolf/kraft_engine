#include "kraft_vulkan_backend.h"

#include "core/kraft_application.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "containers/array.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/shaderfx/kraft_shaderfx_types.h"
#include "renderer/vulkan/kraft_vulkan_device.h"
#include "renderer/vulkan/kraft_vulkan_surface.h"
#include "renderer/vulkan/kraft_vulkan_swapchain.h"
#include "renderer/vulkan/kraft_vulkan_renderpass.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"
#include "renderer/vulkan/kraft_vulkan_framebuffer.h"
#include "renderer/vulkan/kraft_vulkan_fence.h"
#include "renderer/vulkan/kraft_vulkan_shader.h"
#include "renderer/vulkan/kraft_vulkan_pipeline.h"
#include "renderer/vulkan/kraft_vulkan_buffer.h"
#include "renderer/vulkan/kraft_vulkan_index_buffer.h"
#include "renderer/vulkan/kraft_vulkan_vertex_buffer.h"
#include "renderer/vulkan/kraft_vulkan_texture.h"
#include "renderer/vulkan/kraft_vulkan_helpers.h"

// TODO: (amn) This is just temporary
#include "renderer/vulkan/tests/simple_scene.h"

namespace kraft::renderer
{

static VulkanContext s_Context = {};
static VkDescriptorPool GlobalDescriptorPool;
static VkDescriptorSetLayout GlobalDescriptorSetLayout;
static VkDescriptorSet GlobalDescriptorSet[3];

VulkanContext* VulkanRendererBackend::GetContext()
{
    return &s_Context;
}

static bool createBuffers();
static void createCommandBuffers();
static void destroyCommandBuffers();
static void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* renderPass);
static void destroyFramebuffers(VulkanSwapchain* swapchain);
static bool instanceLayerSupported(const char* layer, VkLayerProperties* layerProperties, uint32 count);

bool VulkanRendererBackend::Init(ApplicationConfig* config)
{
    s_Context = {};

    s_Context.AllocationCallbacks = nullptr;
    s_Context.FramebufferWidth = config->WindowWidth;
    s_Context.FramebufferHeight = config->WindowHeight;

    for (int i = 0; i < KRAFT_VULKAN_MAX_GEOMETRIES; i++)
    {
        s_Context.Geometries[i].ID = KRAFT_INVALID_ID;
    }

    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = config->ApplicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = config->ApplicationName;
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);

    VkInstanceCreateInfo instanceCreateInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
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

#ifdef KRAFT_RENDERER_DEBUG
    arrfree(availableLayerProperties);
    {
        // Setup vulkan debug callback messenger
        uint32 logLevel = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    
        VkDebugUtilsMessengerCreateInfoEXT createInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        createInfo.messageSeverity = logLevel;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = VulkanRendererBackend::DebugUtilsMessenger;
        createInfo.pUserData = 0;

        PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_Context.Instance, "vkCreateDebugUtilsMessengerEXT");
        KRAFT_ASSERT_MESSAGE(func, "Failed to get function address for vkCreateDebugUtilsMessengerEXT");

        KRAFT_VK_CHECK(func(s_Context.Instance, &createInfo, s_Context.AllocationCallbacks, &s_Context.DebugMessenger));
    }
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

    CreateVulkanSurface(&s_Context);
    VulkanSelectPhysicalDevice(&s_Context, requirements);
    VulkanCreateLogicalDevice(&s_Context, requirements);
    VulkanCreateSwapchain(&s_Context, s_Context.FramebufferWidth, s_Context.FramebufferHeight);
    VulkanCreateRenderPass(&s_Context, {0.10f, 0.10f, 0.10f, 1.0f}, {0.0f, 0.0f, (float)s_Context.FramebufferWidth, (float)s_Context.FramebufferHeight}, 1.0f, 0, &s_Context.MainRenderPass);

    arrfree(requirements.DeviceExtensionNames);

    createFramebuffers(&s_Context.Swapchain, &s_Context.MainRenderPass);
    createCommandBuffers();

    CreateArray(s_Context.ImageAvailableSemaphores, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.RenderCompleteSemaphores, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.WaitFences, s_Context.Swapchain.ImageCount);
    CreateArray(s_Context.InFlightImageToFenceMap, s_Context.Swapchain.ImageCount);

    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &info, s_Context.AllocationCallbacks, &s_Context.ImageAvailableSemaphores[i]);
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &info, s_Context.AllocationCallbacks, &s_Context.RenderCompleteSemaphores[i]);

        VulkanCreateFence(&s_Context, true, &s_Context.WaitFences[i]);
        s_Context.InFlightImageToFenceMap[i] = &s_Context.WaitFences[i];
    }

    // Global Descriptor Sets
    {
        // globalUniformObject
        VkDescriptorSetLayoutBinding GlobalDescriptorSetLayoutBinding = {};
        GlobalDescriptorSetLayoutBinding.binding = 0;
        GlobalDescriptorSetLayoutBinding.descriptorCount = 1;
        GlobalDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        GlobalDescriptorSetLayoutBinding.pImmutableSamplers = 0;
        GlobalDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo GlobalDescriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        GlobalDescriptorSetLayoutCreateInfo.bindingCount = 1;
        GlobalDescriptorSetLayoutCreateInfo.pBindings = &GlobalDescriptorSetLayoutBinding;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(
            s_Context.LogicalDevice.Handle, 
            &GlobalDescriptorSetLayoutCreateInfo, 
            s_Context.AllocationCallbacks, 
            &GlobalDescriptorSetLayout
        ));

        const uint32 GlobalPoolSize = 128 * s_Context.Swapchain.ImageCount;
        VkDescriptorPoolSize PoolSizes[] =
        {
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
        DescriptorPoolCreateInfo.poolSizeCount = sizeof(PoolSizes) / sizeof(PoolSizes[0]);
        DescriptorPoolCreateInfo.pPoolSizes = &PoolSizes[0];
        DescriptorPoolCreateInfo.maxSets = GlobalPoolSize * sizeof(PoolSizes) / sizeof(PoolSizes[0]);

        vkCreateDescriptorPool(s_Context.LogicalDevice.Handle, &DescriptorPoolCreateInfo, s_Context.AllocationCallbacks, &GlobalDescriptorPool);

        // Allocate enough descriptor sets (= number of swapchain images)
        Array<VkDescriptorSetLayout> DescriptorSets = Array<VkDescriptorSetLayout>(s_Context.Swapchain.ImageCount, GlobalDescriptorSetLayout);

        VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        DescriptorSetAllocateInfo.descriptorPool = GlobalDescriptorPool;
        DescriptorSetAllocateInfo.descriptorSetCount = DescriptorSets.Length;
        DescriptorSetAllocateInfo.pSetLayouts = &DescriptorSets[0];

        KRAFT_VK_CHECK(vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &DescriptorSetAllocateInfo, GlobalDescriptorSet));
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

    VulkanDestroyBuffer(&s_Context, &s_Context.VertexBuffer);
    VulkanDestroyBuffer(&s_Context, &s_Context.IndexBuffer);

    vkDestroyDescriptorPool(s_Context.LogicalDevice.Handle, GlobalDescriptorPool, s_Context.AllocationCallbacks);
    vkDestroyDescriptorSetLayout(s_Context.LogicalDevice.Handle, GlobalDescriptorSetLayout, s_Context.AllocationCallbacks);

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
    destroyCommandBuffers();

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

    return true;
}

bool VulkanRendererBackend::BeginFrame(float64 deltaTime)
{
    // if (!VulkanWaitForFence(&s_Context, &s_Context.WaitFences[s_Context.Swapchain.CurrentFrame], UINT64_MAX))
    // {
    //     KERROR("[VulkanRendererBackend::BeginFrame]: VulkanWaitForFence failed");
    //     return false;
    // }

    // Acquire the next image
    if (!VulkanAcquireNextImageIndex(&s_Context, UINT64_MAX, s_Context.ImageAvailableSemaphores[s_Context.Swapchain.CurrentFrame], 0, &s_Context.CurrentSwapchainImageIndex))
    {
        return false;
    }

    VulkanFence* fence = s_Context.InFlightImageToFenceMap[s_Context.CurrentSwapchainImageIndex];
    if (!VulkanWaitForFence(&s_Context, fence, UINT64_MAX))
    {
        KERROR("[VulkanRendererBackend::BeginFrame]: VulkanWaitForFence failed");
        return false;
    }

    VulkanResetFence(&s_Context, fence);

    // Record commands
    VulkanCommandBuffer* buffer = &s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];
    VulkanResetCommandBuffer(buffer);

    VulkanBeginCommandBuffer(buffer, false, false, false);

    VulkanBeginRenderPass(buffer, &s_Context.MainRenderPass, s_Context.Swapchain.Framebuffers[s_Context.CurrentSwapchainImageIndex].Handle, VK_SUBPASS_CONTENTS_INLINE);
    
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = (float32)s_Context.FramebufferHeight;
    viewport.width = (float32)s_Context.FramebufferWidth;
    viewport.height = -(float32)s_Context.FramebufferHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(buffer->Handle, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent = {s_Context.FramebufferWidth, s_Context.FramebufferHeight};

    vkCmdSetScissor(buffer->Handle, 0, 1, &scissor);

    s_Context.MainRenderPass.Rect.z = (float32)s_Context.FramebufferWidth;
    s_Context.MainRenderPass.Rect.w = (float32)s_Context.FramebufferHeight;

    // Debug Stuff
    RenderTestScene(&s_Context, buffer);

    return true;
}

bool VulkanRendererBackend::EndFrame(float64 deltaTime)
{
    VulkanCommandBuffer* buffer = &s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];
    VulkanEndRenderPass(buffer, &s_Context.MainRenderPass);
    VulkanEndCommandBuffer(buffer);

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &buffer->Handle;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &s_Context.ImageAvailableSemaphores[s_Context.Swapchain.CurrentFrame];
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &s_Context.RenderCompleteSemaphores[s_Context.Swapchain.CurrentFrame];
    info.pWaitDstStageMask = &waitStageMask;

    KRAFT_VK_CHECK(vkQueueSubmit(s_Context.LogicalDevice.GraphicsQueue, 1, &info, s_Context.InFlightImageToFenceMap[s_Context.CurrentSwapchainImageIndex]->Handle));
    
    VulkanSetCommandBufferSubmitted(buffer);

    VulkanPresentSwapchain(&s_Context, s_Context.LogicalDevice.PresentQueue, s_Context.RenderCompleteSemaphores[s_Context.Swapchain.CurrentFrame], s_Context.CurrentSwapchainImageIndex);

    // To make sure that the next frame which we will render to is finished
    // s_Context.Swapchain.CurrentFrame is incremented in VulkanPresentSwapchain()
    // vkWaitForFences(s_Context.LogicalDevice.Handle, 1, &s_Context.WaitFences[s_Context.Swapchain.CurrentFrame].Handle, true, UINT64_MAX);

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
    
    s_Context.MainRenderPass.Rect = {0.0f, 0.0f, (float)s_Context.FramebufferWidth, (float)s_Context.FramebufferHeight};
    
    createCommandBuffers();
    createFramebuffers(&s_Context.Swapchain, &s_Context.MainRenderPass);
}

void VulkanRendererBackend::CreateRenderPipeline(Shader* Shader, int PassIndex)
{
    const ShaderEffect& Effect = Shader->ShaderEffect;
    int RenderPassesCount = Effect.RenderPasses.Length;

    const RenderPassDefinition& Pass = Effect.RenderPasses[PassIndex];
    VkGraphicsPipelineCreateInfo PipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    
    // Shader stages
    int ShaderStagesCount = Pass.ShaderStages.Length;
    auto PipelineShaderStageCreateInfos = Array<VkPipelineShaderStageCreateInfo>(ShaderStagesCount);
    Array<VkShaderModule> ShaderModules(ShaderStagesCount); 
    for (int j = 0; j < ShaderStagesCount; j++)
    {
        const RenderPassDefinition::ShaderDefinition& Shader = Pass.ShaderStages[j];
        VkShaderModule ShaderModule;
        VulkanCreateShaderModule(&s_Context, Shader.CodeFragment.Code.Buffer, Shader.CodeFragment.Code.Length, &ShaderModule);
        KASSERT(ShaderModule);

        VkPipelineShaderStageCreateInfo ShaderStageInfo = {};
        ShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ShaderStageInfo.module = ShaderModule;
        ShaderStageInfo.pName = "main";
        ShaderStageInfo.stage = ToVulkanShaderStage(Shader.Stage);

        PipelineShaderStageCreateInfos[j] = ShaderStageInfo;
        ShaderModules[j] = ShaderModule;
    }

    // Viewport info
    VkViewport Viewport;
    Viewport.x = 0;
    Viewport.y = (float32)s_Context.FramebufferHeight;
    Viewport.width = (float32)s_Context.FramebufferWidth;
    Viewport.height = -(float32)s_Context.FramebufferHeight;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;

    VkRect2D Scissor = {};
    Scissor.extent = {s_Context.FramebufferWidth, s_Context.FramebufferHeight};

    // Input bindings and attributes
    int InputBindingsCount = Pass.VertexLayout->InputBindings.Length;
    auto InputBindingsDesc = Array<VkVertexInputBindingDescription>(InputBindingsCount);
    for (int j = 0; j < InputBindingsCount; j++)
    {
        const VertexInputBinding& InputBinding = Pass.VertexLayout->InputBindings[j];
        InputBindingsDesc[j].binding = InputBinding.Binding;
        InputBindingsDesc[j].inputRate = InputBinding.InputRate == VertexInputRate::PerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        InputBindingsDesc[j].stride = InputBinding.Stride;
    }

    int AttributesCount = Pass.VertexLayout->Attributes.Length;
    auto AttributesDesc = Array<VkVertexInputAttributeDescription>(AttributesCount);
    for (int j = 0; j < AttributesCount; j++)
    {
        const VertexAttribute& VertexAttribute = Pass.VertexLayout->Attributes[j];
        AttributesDesc[j].location = VertexAttribute.Location;
        AttributesDesc[j].format = ToVulkanFormat(VertexAttribute.Format);
        AttributesDesc[j].binding = VertexAttribute.Binding;
        AttributesDesc[j].offset = VertexAttribute.Offset;
    }

    // Shader resources
    // Both global and instance ubos will be written separately, so the offsets must be aligned
    uint64 UBOSize = GetAligned(sizeof(GlobalUniformData), s_Context.PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment);
    Shader->GlobalUBOOffset = 0;
    Shader->GlobalUBOStride = UBOSize;
    Shader->InstanceUBOOffset = UBOSize;

    int ResourceBindingsCount = Pass.Resources->ResourceBindings.Length;
    int ConstantBuffersCount = Pass.ConstantBuffers->Fields.Length;

    auto DescriptorSetLayoutBindings = Array<VkDescriptorSetLayoutBinding>(ResourceBindingsCount);
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
        DescriptorSetLayoutBindings[j].stageFlags = ToVulkanShaderStage(Binding.Stage);

        if (Binding.Type == ResourceType::UniformBuffer)
        {
            UBOSize += Binding.Size;
        }
    }

    UBOSize = GetAligned(UBOSize, s_Context.PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment);
    Shader->InstanceUBOStride = UBOSize - Shader->GlobalUBOStride;

    // Constant Buffers
    uint64 ConstantBufferSize = 0;
    auto PushConstantRanges = Array<VkPushConstantRange>(ConstantBuffersCount);
    for (int j = 0; j < ConstantBuffersCount; j++)
    {
        const ConstantBufferEntry& Entry = Pass.ConstantBuffers->Fields[j];
        uint64 AlignedSize = GetAligned(ShaderDataType::SizeOf(Entry.Type), 4);
        PushConstantRanges[j].size   = AlignedSize;
        PushConstantRanges[j].offset = ConstantBufferSize;
        PushConstantRanges[j].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        ConstantBufferSize += AlignedSize;
    }

    KASSERTM(ConstantBufferSize <= 128, "ConstantBuffers cannot be larger than 128 bytes");

    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    DescriptorSetLayoutCreateInfo.bindingCount = DescriptorSetLayoutBindings.Length;
    DescriptorSetLayoutCreateInfo.pBindings = &DescriptorSetLayoutBindings[0];

    VkDescriptorSetLayout LocalDescriptorSetLayout;
    KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(s_Context.LogicalDevice.Handle, &DescriptorSetLayoutCreateInfo, s_Context.AllocationCallbacks, &LocalDescriptorSetLayout));

    VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};
    ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportStateCreateInfo.pViewports = &Viewport;
    ViewportStateCreateInfo.viewportCount = 1;
    ViewportStateCreateInfo.pScissors = &Scissor;
    ViewportStateCreateInfo.scissorCount = 1;
    PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;

    VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo = {};
    RasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    RasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    RasterizationStateCreateInfo.polygonMode = ToVulkanPolygonMode(Pass.RenderState->PolygonMode);
    RasterizationStateCreateInfo.lineWidth = Pass.RenderState->LineWidth;
    RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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
    DepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    DepthStencilStateCreateInfo.depthWriteEnable = Pass.RenderState->ZWriteEnable;
    DepthStencilStateCreateInfo.depthCompareOp = ToVulkanCompareOp(Pass.RenderState->ZTestOperation);
    DepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    DepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    PipelineCreateInfo.pDepthStencilState = &DepthStencilStateCreateInfo;

    VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {};
    ColorBlendAttachmentState.blendEnable = VK_TRUE;
    ColorBlendAttachmentState.srcColorBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.SrcColorBlendFactor);
    ColorBlendAttachmentState.dstColorBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.DstColorBlendFactor);
    ColorBlendAttachmentState.colorBlendOp = ToVulkanBlendOp(Pass.RenderState->BlendMode.ColorBlendOperation);
    ColorBlendAttachmentState.srcAlphaBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.SrcAlphaBlendFactor);
    ColorBlendAttachmentState.dstAlphaBlendFactor = ToVulkanBlendFactor(Pass.RenderState->BlendMode.DstAlphaBlendFactor);
    ColorBlendAttachmentState.alphaBlendOp = ToVulkanBlendOp(Pass.RenderState->BlendMode.AlphaBlendOperation);
    ColorBlendAttachmentState.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | 
        VK_COLOR_COMPONENT_G_BIT | 
        VK_COLOR_COMPONENT_B_BIT | 
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};
    ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendStateCreateInfo.attachmentCount = 1;
    ColorBlendStateCreateInfo.pAttachments = &ColorBlendAttachmentState;
    ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;

    VkDynamicState DynamicStates[] = 
    {
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
    VertexInputStateCreateInfo.vertexBindingDescriptionCount = InputBindingsDesc.Length;
    VertexInputStateCreateInfo.pVertexAttributeDescriptions = &AttributesDesc[0];
    VertexInputStateCreateInfo.vertexAttributeDescriptionCount = AttributesDesc.Length;
    PipelineCreateInfo.pVertexInputState = &VertexInputStateCreateInfo;

    VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo = {};
    InputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;
    InputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineCreateInfo.pInputAssemblyState = &InputAssemblyStateCreateInfo;

    VkDescriptorSetLayout DescriptorSetLayouts[] = 
    {
        GlobalDescriptorSetLayout,
        LocalDescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo LayoutCreateInfo = {};
    LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    LayoutCreateInfo.setLayoutCount = sizeof(DescriptorSetLayouts) / sizeof(DescriptorSetLayouts[0]);
    LayoutCreateInfo.pSetLayouts = &DescriptorSetLayouts[0];
    LayoutCreateInfo.pushConstantRangeCount = PushConstantRanges.Length;
    LayoutCreateInfo.pPushConstantRanges = &PushConstantRanges[0];

    VkPipelineLayout PipelineLayout;
    KRAFT_VK_CHECK(vkCreatePipelineLayout(s_Context.LogicalDevice.Handle, &LayoutCreateInfo, s_Context.AllocationCallbacks, &PipelineLayout));
    KASSERT(PipelineLayout);
    PipelineCreateInfo.layout = PipelineLayout;

    PipelineCreateInfo.stageCount = PipelineShaderStageCreateInfos.Length;
    PipelineCreateInfo.pStages = &PipelineShaderStageCreateInfos[0];
    PipelineCreateInfo.pTessellationState = 0;

    PipelineCreateInfo.renderPass = s_Context.MainRenderPass.Handle;
    PipelineCreateInfo.subpass = 0;
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;

    VkPipeline Pipeline;
    KRAFT_VK_CHECK(vkCreateGraphicsPipelines(
        s_Context.LogicalDevice.Handle, 
        VK_NULL_HANDLE, 1, &PipelineCreateInfo, 
        s_Context.AllocationCallbacks, 
        &Pipeline));

    KASSERT(Pipeline);

    VulkanShader* VulkanShaderData = (VulkanShader*)Malloc(sizeof(VulkanShader), MEMORY_TAG_RENDERER, true);

    // Allocate the local descriptor sets
    // We will have at most 3 images in the swapchain, so we assume that to be
    // the upper bound of the arrays
    const uint32 MaxSwapchainImages = 3;
    VulkanShaderData->ShaderResources.DescriptorSetLayouts = Array<VkDescriptorSetLayout>();
    VulkanShaderData->ShaderResources.DescriptorSets = Array<VkDescriptorSet>(MaxSwapchainImages);
    VulkanShaderData->ShaderResources.DescriptorSetLayouts.Push(LocalDescriptorSetLayout);

    VkDescriptorSetLayout LocalLayouts[MaxSwapchainImages] = 
    {
        LocalDescriptorSetLayout,
        LocalDescriptorSetLayout,
        LocalDescriptorSetLayout,
    };

    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    DescriptorSetAllocateInfo.descriptorPool = GlobalDescriptorPool;
    DescriptorSetAllocateInfo.descriptorSetCount = MaxSwapchainImages;
    DescriptorSetAllocateInfo.pSetLayouts = &LocalLayouts[0];

    VkResult AllocationResult = vkAllocateDescriptorSets(s_Context.LogicalDevice.Handle, &DescriptorSetAllocateInfo, VulkanShaderData->ShaderResources.DescriptorSets.Data());
    if (AllocationResult == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        // TODO (amn): Solve this
        KFATAL("Pool ran out of memory");
    }

    // Create the uniform buffer for data upload
    uint32 extraMemoryFlags = s_Context.PhysicalDevice.SupportsDeviceLocalHostVisible ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
    VulkanCreateBuffer(&s_Context, UBOSize, VK_SHARING_MODE_EXCLUSIVE, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | extraMemoryFlags,
        true, &VulkanShaderData->ShaderResources.UniformBuffer);

    // Map the buffer memory
    VulkanShaderData->ShaderResources.UniformBufferMemory = VulkanMapBufferMemory(&s_Context, &VulkanShaderData->ShaderResources.UniformBuffer, VK_WHOLE_SIZE, 0);

    // Invalidate all the descriptor binding states so we write to them correctly on the first draw call
    for (int BindingIndex = 0; BindingIndex < ResourceBindingsCount; BindingIndex++)
    {
        for (int j = 0; j < MaxSwapchainImages; j++)
        {
            VulkanShaderData->ShaderResources.DescriptorStates[BindingIndex].Generations[j] = KRAFT_INVALID_ID_UINT8;
        }
    }

    Shader->RendererData = VulkanShaderData;
    VulkanShaderData->Pipeline = Pipeline;
    VulkanShaderData->PipelineLayout = PipelineLayout;
    VulkanShaderData->ShaderResources = VulkanShaderData->ShaderResources;

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
        VulkanShader* VulkanShaderData = (VulkanShader*)Shader->RendererData;
        VulkanShaderResources ShaderResources = VulkanShaderData->ShaderResources;
        VkPipeline Pipeline = VulkanShaderData->Pipeline;
        VkPipelineLayout PipelineLayout = VulkanShaderData->PipelineLayout;

        VulkanDestroyBuffer(&s_Context, &ShaderResources.UniformBuffer);

        for (int i = 0; i < ShaderResources.DescriptorSetLayouts.Length; i++)
        {
            vkDestroyDescriptorSetLayout(s_Context.LogicalDevice.Handle, ShaderResources.DescriptorSetLayouts[i], s_Context.AllocationCallbacks);
        }

        // vkFreeDescriptorSets(s_Context.LogicalDevice.Handle, GlobalDescriptorPool, ShaderResources->DescriptorSets.Length, ShaderResources->DescriptorSets.Data());

        vkDestroyPipeline(s_Context.LogicalDevice.Handle, Pipeline, s_Context.AllocationCallbacks);
        vkDestroyPipelineLayout(s_Context.LogicalDevice.Handle, PipelineLayout, s_Context.AllocationCallbacks);

        Free(Shader->RendererData, sizeof(VulkanShader), MEMORY_TAG_RENDERER);
        Shader->RendererData = 0;
    }
}

void VulkanRendererBackend::CreateTexture(uint8* data, Texture* texture)
{
    VulkanCreateTexture(&s_Context, data, texture);
}

void VulkanRendererBackend::DestroyTexture(Texture* texture)
{
    VulkanDestroyTexture(&s_Context, texture);
}

void VulkanRendererBackend::UseShader(const Shader* Shader)
{
    VulkanShader* VulkanShaderData = (VulkanShader*)Shader->RendererData;
    VkPipeline Pipeline = VulkanShaderData->Pipeline;
    VulkanCommandBuffer Buffer = s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];
    vkCmdBindPipeline(Buffer.Handle, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
}

void VulkanRendererBackend::SetUniform(Shader* Shader, const ShaderUniform& Uniform, void* Value, bool Invalidate)
{
    VulkanShader* VulkanShaderData = (VulkanShader*)Shader->RendererData;
    VkPipeline Pipeline = VulkanShaderData->Pipeline;
    VkPipelineLayout PipelineLayout = VulkanShaderData->PipelineLayout;
    VulkanCommandBuffer Buffer = s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];

    if (Uniform.Scope == ShaderUniformScope::Local)
    {
        vkCmdPushConstants(Buffer.Handle, PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, Uniform.Offset, Uniform.Stride, Value);
    }
    else
    {
        if (Uniform.Type == ResourceType::Sampler)
        {
            Shader->Textures[Uniform.Offset] = (Texture*)Value;
        }
        else
        {
            void* Offset = (char*)VulkanShaderData->ShaderResources.UniformBufferMemory + Uniform.Offset;
            MemCpy(Offset, Value, Uniform.Stride);
        }

        if (Uniform.Scope == ShaderUniformScope::Instance && Invalidate)
        {
            const int MaxSwapchainImages = 3;
            for (int j = 0; j < MaxSwapchainImages; j++)
            {
                VulkanShaderData->ShaderResources.DescriptorStates[Uniform.Location].Generations[j] = KRAFT_INVALID_ID_UINT8;
            }
        }
    }
}

void VulkanRendererBackend::ApplyGlobalShaderProperties(Shader* Shader)
{
    VulkanShader* ShaderData = (VulkanShader*)Shader->RendererData;

    VkDescriptorBufferInfo DescriptorBufferInfo;
    DescriptorBufferInfo.buffer = ShaderData->ShaderResources.UniformBuffer.Handle;
    DescriptorBufferInfo.offset = Shader->GlobalUBOOffset;
    DescriptorBufferInfo.range = Shader->GlobalUBOStride;

    VkWriteDescriptorSet DescriptorWriteInfo = {};
    DescriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWriteInfo.descriptorCount = 1;
    DescriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    DescriptorWriteInfo.dstSet = GlobalDescriptorSet[s_Context.CurrentSwapchainImageIndex];
    DescriptorWriteInfo.dstBinding = 0;
    DescriptorWriteInfo.dstArrayElement = 0;
    DescriptorWriteInfo.pBufferInfo = &DescriptorBufferInfo;

    vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, 1, &DescriptorWriteInfo, 0, 0);
    vkCmdBindDescriptorSets(
        s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex].Handle, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        ShaderData->PipelineLayout, 
        0, 1,
        &GlobalDescriptorSet[s_Context.CurrentSwapchainImageIndex], 
        0, 0
    );
}

void VulkanRendererBackend::ApplyInstanceShaderProperties(Shader* Shader)
{
    uint32 WriteCount = 0;
    VkWriteDescriptorSet WriteInfos[32] = {};
    VkWriteDescriptorSet DescriptorWriteInfo = {};

    VulkanShader* ShaderData = (VulkanShader*)Shader->RendererData;
    VulkanShaderResources* ShaderResources = &ShaderData->ShaderResources;
    
    uint32 BindingIndex = 0;
    // Material data
    if (ShaderResources->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex] == KRAFT_INVALID_ID_UINT8)
    {
        VkDescriptorBufferInfo DescriptorBufferInfo = {};
        DescriptorBufferInfo.buffer = ShaderResources->UniformBuffer.Handle;
        DescriptorBufferInfo.offset = Shader->InstanceUBOOffset;
        DescriptorBufferInfo.range = Shader->InstanceUBOStride;

        DescriptorWriteInfo = {};
        DescriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWriteInfo.descriptorCount = 1;
        DescriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        DescriptorWriteInfo.dstSet = ShaderResources->DescriptorSets[s_Context.CurrentSwapchainImageIndex];
        DescriptorWriteInfo.dstBinding = BindingIndex;
        DescriptorWriteInfo.dstArrayElement = 0;
        DescriptorWriteInfo.pBufferInfo = &DescriptorBufferInfo;

        WriteInfos[WriteCount] = DescriptorWriteInfo;
        ShaderResources->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex]++;
        WriteCount++;
    }

    BindingIndex++;

    // Sampler
    if (ShaderResources->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex] == KRAFT_INVALID_ID_UINT8)
    {
        VkDescriptorImageInfo DescriptorImageInfo[32] = {};
        for (int i = 0; i < Shader->Textures.Length; i++)
        {
            kraft::Texture* Texture = Shader->Textures[i];
            VulkanTexture* VkTexture = (VulkanTexture*)Texture->RendererData;

            KASSERT(VkTexture->Image.Handle);
            KASSERT(VkTexture->Image.View);

            DescriptorImageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            DescriptorImageInfo[i].imageView = VkTexture->Image.View;
            DescriptorImageInfo[i].sampler = VkTexture->Sampler;
        }

        DescriptorWriteInfo = {};
        DescriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        DescriptorWriteInfo.descriptorCount = Shader->Textures.Length;
        DescriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        DescriptorWriteInfo.dstSet = ShaderResources->DescriptorSets[s_Context.CurrentSwapchainImageIndex];
        DescriptorWriteInfo.dstBinding = BindingIndex;
        DescriptorWriteInfo.dstArrayElement = 0;
        DescriptorWriteInfo.pImageInfo = DescriptorImageInfo;

        WriteInfos[WriteCount] = DescriptorWriteInfo;
        ShaderResources->DescriptorStates[BindingIndex].Generations[s_Context.CurrentSwapchainImageIndex]++;
        WriteCount++;
    }
    
    BindingIndex++;
    if (WriteCount > 0)
    {
        vkUpdateDescriptorSets(s_Context.LogicalDevice.Handle, WriteCount, WriteInfos, 0, 0);
    }

    vkCmdBindDescriptorSets(
        s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex].Handle, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        ShaderData->PipelineLayout, 
        1, 1,
        &ShaderResources->DescriptorSets[s_Context.CurrentSwapchainImageIndex], 
        0, 0
    );
}

void VulkanRendererBackend::DrawGeometryData(GeometryRenderData Data)
{
    
}

static bool UploadDataToGPU(VulkanContext* Context, VulkanBuffer DstBuffer, uint32 DstBufferOffset, const void* Data, uint32 Size)
{
    // Create a staging buffer
    VulkanBuffer StagingBuffer;
    if (!VulkanCreateBuffer(Context, Size, VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true, &StagingBuffer))
    {
        return false;
    }

    // Load the given data in the staging buffer
    VulkanLoadDataInBuffer(Context, &StagingBuffer, Data, Size, 0);

    // Copy the staging buffer data to the vertex buffer
    VulkanCopyBufferTo(Context, &StagingBuffer, 0, &DstBuffer, DstBufferOffset, Size, Context->LogicalDevice.GraphicsQueue, Context->GraphicsCommandPool);

    // Destroy the staging buffer
    VulkanDestroyBuffer(Context, &StagingBuffer);

    return true;
}

bool VulkanRendererBackend::CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize)
{
    VulkanGeometryData* GeometryData = nullptr;
    for (int i = 0; i < KRAFT_VULKAN_MAX_GEOMETRIES; i++)
    {
        if (s_Context.Geometries[i].ID == KRAFT_INVALID_ID)
        {
            s_Context.Geometries[i].ID = i;
            Geometry->InternalID = i;
            GeometryData = &s_Context.Geometries[i];
            break;
        }
    }

    if (!GeometryData)
    {
        KERROR("[VulkanRendererBackend::CreateGeometry]: Could not create geometry - Out of memory!");
        return false;
    }

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
    if (!Geometry || Geometry->InternalID == KRAFT_INVALID_ID) return;

    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);

    VulkanGeometryData* GeometryData = &s_Context.Geometries[Geometry->InternalID];

    MemZero(GeometryData, sizeof(VulkanGeometryData));
    GeometryData->ID = KRAFT_INVALID_ID;
}

#ifdef KRAFT_RENDERER_DEBUG
VkBool32 VulkanRendererBackend::DebugUtilsMessenger(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        {
            KERROR("%s", pCallbackData->pMessage);
        } break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        {
            KWARN("%s", pCallbackData->pMessage);
        } break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        {
            KDEBUG("%s", pCallbackData->pMessage);
        } break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        {
            KDEBUG("%s", pCallbackData->pMessage);
        } break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: break;
    }

    return VK_FALSE;
}
#endif

bool createBuffers()
{
    const uint64 VertexBufferSize = sizeof(Vertex3D) * 1024 * 1024;
    if (!VulkanCreateBuffer(&s_Context, VertexBufferSize, VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, &s_Context.VertexBuffer))
    {
        return false;
    }

    const uint64 IndexBufferSize = sizeof(uint32) * 1024 * 1024;
    if (!VulkanCreateBuffer(&s_Context, IndexBufferSize, VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, &s_Context.IndexBuffer))
    {
        return false;
    }

    return true;
}

void createCommandBuffers()
{
    if (!s_Context.GraphicsCommandBuffers)
    {
        CreateArray(s_Context.GraphicsCommandBuffers, s_Context.Swapchain.ImageCount);
    }

    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        if (s_Context.GraphicsCommandBuffers[i].Handle)
        {
            VulkanFreeCommandBuffer(&s_Context, s_Context.GraphicsCommandPool, &s_Context.GraphicsCommandBuffers[i]);
        }
        
        MemZero(&s_Context.GraphicsCommandBuffers[i], sizeof(VulkanCommandBuffer));
        VulkanAllocateCommandBuffer(&s_Context, s_Context.GraphicsCommandPool, true, &s_Context.GraphicsCommandBuffers[i]);
    }

    KDEBUG("[VulkanRendererBackend::Init]: Graphics command buffers created");
}

void destroyCommandBuffers()
{
    if (s_Context.GraphicsCommandBuffers)
    {
        for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
        {
            if (s_Context.GraphicsCommandBuffers[i].Handle)
            {
                VulkanFreeCommandBuffer(&s_Context, s_Context.GraphicsCommandPool, &s_Context.GraphicsCommandBuffers[i]);
            }

            s_Context.GraphicsCommandBuffers[i].Handle = 0;
        }
    }

    arrfree(s_Context.GraphicsCommandBuffers);
    s_Context.GraphicsCommandBuffers = 0;
}

void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* renderPass)
{
    if (!swapchain->Framebuffers)
    {
        arrsetlen(swapchain->Framebuffers, swapchain->ImageCount);
        MemZero(swapchain->Framebuffers, sizeof(VulkanFramebuffer) * swapchain->ImageCount);
    }

    for (uint32 i = 0; i < swapchain->ImageCount; ++i)
    {
        if (swapchain->Framebuffers[i].Handle)
        {
            VulkanDestroyFramebuffer(&s_Context, &swapchain->Framebuffers[i]);
        }

        VkImageView* attachments = 0;
        arrput(attachments, swapchain->ImageViews[i]);
        arrput(attachments, swapchain->DepthAttachment.View);

        VulkanCreateFramebuffer(&s_Context, s_Context.FramebufferWidth, s_Context.FramebufferHeight, renderPass, attachments, &swapchain->Framebuffers[i]);
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

namespace kraft
{

using namespace renderer;

}