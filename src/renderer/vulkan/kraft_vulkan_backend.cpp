#include "kraft_vulkan_backend.h"

#include "core/kraft_application.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "containers/array.h"
#include "renderer/kraft_renderer_types.h"
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

namespace kraft
{

static VulkanContext s_Context;

static void createCommandBuffers();
static void destroyCommandBuffers();
static void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* renderPass);
static void destroyFramebuffers(VulkanSwapchain* swapchain);

// TODO: Temp
static VulkanBuffer VertexBuffer;
static VulkanBuffer IndexBuffer;
static uint32 IndexCount = 0;

bool VulkanRendererBackend::Init(ApplicationConfig* config)
{
    s_Context = {};
    s_Context.AllocationCallbacks = nullptr;
    s_Context.FramebufferWidth = config->WindowWidth;
    s_Context.FramebufferHeight = config->WindowHeight;

    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = config->ApplicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = config->ApplicationName;
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);

    VkInstanceCreateInfo instanceCreateInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.pApplicationInfo = &appInfo;

    // Extensions
    const char** extensions = nullptr;
    arrput(extensions, VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(KRAFT_PLATFORM_WINDOWS)
    arrput(extensions, "VK_KHR_win32_surface");
#elif defined(KRAFT_PLATFORM_MACOS)
    arrput(extensions, "VK_EXT_metal_surface");
#elif defined(KRAFT_PLATFORM_LINUX)
    arrput(extensions, "VK_KHR_xcb_surface");
#endif

#ifdef KRAFT_DEBUG
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

#ifdef KRAFT_DEBUG
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
        bool found = false;
        for (uint32 j = 0; j < availableLayers; ++j)
        {
            if (strcmp(availableLayerProperties[j].layerName, layers[i]) == 0)
            {
                found = true;
                break;
            }
        }

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

#ifdef KRAFT_DEBUG
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
    
    KSUCCESS("[VulkanRendererBackend::Init]: Backend init success!");

    // DEBUG Stuff
    VkShaderModule vertex, fragment;
    VulkanCreateShaderModule(&s_Context, "res/shaders/vertex.vert.spv", &vertex);
    assert(vertex);

    VulkanCreateShaderModule(&s_Context, "res/shaders/fragment.frag.spv", &fragment);
    assert(fragment);

    VkPipelineShaderStageCreateInfo vertexShaderStage = {};
    vertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStage.module = vertex;
    vertexShaderStage.pName = "main";
    vertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineShaderStageCreateInfo fragmentShaderStage = {};
    fragmentShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStage.module = fragment;
    fragmentShaderStage.pName = "main";
    fragmentShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineShaderStageCreateInfo stages[] = {
        vertexShaderStage,
        fragmentShaderStage
    };

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = (float32)s_Context.FramebufferHeight;
    viewport.width = (float32)s_Context.FramebufferWidth;
    viewport.height = -(float32)s_Context.FramebufferHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent = {s_Context.FramebufferWidth, s_Context.FramebufferHeight};
    
    VkVertexInputAttributeDescription inputAttributeDesc[2];
    inputAttributeDesc[0].location = 0;
    inputAttributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttributeDesc[0].binding = 0;
    inputAttributeDesc[0].offset = offsetof(Vertex3D, Position);

    inputAttributeDesc[1].location = 1;
    inputAttributeDesc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    inputAttributeDesc[1].binding = 0;
    inputAttributeDesc[1].offset = offsetof(Vertex3D, Color);

    VulkanPipelineDescription pipelineDesc = {};
    pipelineDesc.IsWireframe = false;
    pipelineDesc.ShaderStageCount = sizeof(stages) / sizeof(stages[0]);
    pipelineDesc.ShaderStages = stages;
    pipelineDesc.Viewport = viewport;
    pipelineDesc.Scissor = scissor;
    pipelineDesc.Attributes = inputAttributeDesc;
    pipelineDesc.AttributeCount = sizeof(inputAttributeDesc) / sizeof(inputAttributeDesc[0]);

    s_Context.GraphicsPipeline = {};
    VulkanCreateGraphicsPipeline(&s_Context, &s_Context.MainRenderPass, pipelineDesc, &s_Context.GraphicsPipeline);
    assert(s_Context.GraphicsPipeline.Handle);

    VulkanDestroyShaderModule(&s_Context, &vertex);
    VulkanDestroyShaderModule(&s_Context, &fragment);
    
    // #5865f2
    // Vec3f color = Vec3f(0x58 / 255.0f, 0x65 / 255.0f, 0xf2 / 255.0f);
    Vec4f color = KRGBA(0x58, 0x65, 0xf2, 0x1f);
    Vec4f colorB = KRGBA(0xff, 0x6b, 0x6b, 0xff);
    // Vertex and index buffers
    Vertex3D verts[] = {
        {Vec3f(+0.5f, +0.5f, +0.0f), colorB},
        {Vec3f(-0.5f, -0.5f, +0.0f), color},
        {Vec3f(+0.5f, -0.5f, +0.0f), color},
        {Vec3f(-0.5f, +0.5f, +0.0f), colorB},
    };

    uint32 indices[] = {0, 1, 2, 3, 1, 0};
    IndexCount = sizeof(indices) / sizeof(indices[0]);
    VulkanCreateVertexBuffer(&s_Context, verts, sizeof(verts), &VertexBuffer);
    VulkanCreateIndexBuffer(&s_Context, indices, sizeof(indices), &IndexBuffer);

    return true;
}

bool VulkanRendererBackend::Shutdown()
{
    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);

    VulkanDestroyBuffer(&s_Context, &VertexBuffer);
    VulkanDestroyBuffer(&s_Context, &IndexBuffer);

    VulkanDestroyPipeline(&s_Context, &s_Context.GraphicsPipeline);
    s_Context.GraphicsPipeline = {};

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

    VulkanBeginRenderPass(buffer, &s_Context.MainRenderPass, s_Context.Swapchain.Framebuffers[s_Context.CurrentSwapchainImageIndex].Handle);
    
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

    VulkanBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, &s_Context.GraphicsPipeline);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(buffer->Handle, 0, 1, &VertexBuffer.Handle, offsets);
    vkCmdBindIndexBuffer(buffer->Handle, IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);

    // vkCmdDraw(buffer->Handle, 3, 1, 0, 0);
    vkCmdDrawIndexed(buffer->Handle, IndexCount, 1, 0, 0, 0);
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

#ifdef KRAFT_DEBUG
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
    }

    return VK_FALSE;
}
#endif

void createCommandBuffers()
{
    if (!s_Context.GraphicsCommandBuffers)
    {
        arrsetlen(s_Context.GraphicsCommandBuffers, s_Context.Swapchain.ImageCount);
        for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
        {
            MemZero(&s_Context.GraphicsCommandBuffers[i], sizeof(VulkanCommandBuffer));
        }
    }

    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        if (s_Context.GraphicsCommandBuffers[i].Handle)
        {
            VulkanFreeCommandBuffer(&s_Context, s_Context.LogicalDevice.GraphicsCommandPool, &s_Context.GraphicsCommandBuffers[i]);
        }
        
        MemZero(&s_Context.GraphicsCommandBuffers[i], sizeof(VulkanCommandBuffer));
        VulkanAllocateCommandBuffer(&s_Context, s_Context.LogicalDevice.GraphicsCommandPool, true, &s_Context.GraphicsCommandBuffers[i]);
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
                VulkanFreeCommandBuffer(&s_Context, s_Context.LogicalDevice.GraphicsCommandPool, &s_Context.GraphicsCommandBuffers[i]);
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

}