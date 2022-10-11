#include "kraft_vulkan_backend.h"

#include "core/kraft_application.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "containers/array.h"
#include "renderer/vulkan/kraft_vulkan_device.h"
#include "renderer/vulkan/kraft_vulkan_surface.h"
#include "renderer/vulkan/kraft_vulkan_swapchain.h"
#include "renderer/vulkan/kraft_vulkan_renderpass.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"
#include "renderer/vulkan/kraft_vulkan_framebuffer.h"
#include "renderer/vulkan/kraft_vulkan_fence.h"

namespace kraft
{

static VulkanContext s_Context;

static void createCommandBuffers();
static void destroyCommandBuffers();
static void createFramebuffers(VulkanSwapchain* swapchain, VulkanRenderPass* renderPass);
static void destroyFramebuffers(VulkanSwapchain* swapchain);

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
    arrfree(availableLayerProperties);

#ifdef KRAFT_DEBUG
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
    VulkanPhysicalDeviceRequirements requirements;
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

    for (uint32 i = 0; i < s_Context.Swapchain.ImageCount; ++i)
    {
        VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &info, s_Context.AllocationCallbacks, &s_Context.ImageAvailableSemaphores[i]);
        vkCreateSemaphore(s_Context.LogicalDevice.Handle, &info, s_Context.AllocationCallbacks, &s_Context.RenderCompleteSemaphores[i]);

        VulkanCreateFence(&s_Context, true, &s_Context.WaitFences[i]);
    }
    
    KSUCCESS("[VulkanRendererBackend::Init]: Backend init success!");
    return true;
}

bool VulkanRendererBackend::Shutdown()
{
    vkDeviceWaitIdle(s_Context.LogicalDevice.Handle);
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
    if (!VulkanWaitForFence(&s_Context, &s_Context.WaitFences[s_Context.Swapchain.CurrentFrame], UINT64_MAX))
    {
        KERROR("[VulkanRendererBackend::BeginFrame]: VulkanWaitForFence failed");
        return false;
    }

    // Acquire the next image
    if (!VulkanAcquireNextImageIndex(&s_Context, UINT64_MAX, s_Context.ImageAvailableSemaphores[s_Context.Swapchain.CurrentFrame], 0, &s_Context.CurrentSwapchainImageIndex))
    {
        return false;
    }

    // Record commands
    VulkanCommandBuffer* buffer = &s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];
    VulkanResetCommandBuffer(buffer);
    VulkanBeginCommandBuffer(buffer, false, false, false);

    VkViewport viewport;
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

    VulkanBeginRenderPass(buffer, &s_Context.MainRenderPass, s_Context.Swapchain.Framebuffers[s_Context.CurrentSwapchainImageIndex].Handle);

    return true;
}

bool VulkanRendererBackend::EndFrame(float64 deltaTime)
{
    VulkanCommandBuffer* buffer = &s_Context.GraphicsCommandBuffers[s_Context.CurrentSwapchainImageIndex];
    VulkanEndRenderPass(buffer, &s_Context.MainRenderPass);
    VulkanEndCommandBuffer(buffer);

    VulkanResetFence(&s_Context, &s_Context.WaitFences[s_Context.Swapchain.CurrentFrame]);

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

    KRAFT_VK_CHECK(vkQueueSubmit(s_Context.LogicalDevice.GraphicsQueue, 1, &info, s_Context.WaitFences[s_Context.Swapchain.CurrentFrame].Handle));
    
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