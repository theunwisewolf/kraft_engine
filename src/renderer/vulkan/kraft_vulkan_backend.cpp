#include "kraft_vulkan_backend.h"

#include "core/kraft_log.h"
#include "containers/array.h"
#include "renderer/vulkan/kraft_vulkan_device.h"
#include "platform/kraft_platform.h"

#include <vulkan/vulkan.h>

// Platform specific includes
#if defined(KRAFT_PLATFORM_WINDOWS)
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(KRAFT_PLATFORM_MACOS)
#include <vulkan/vulkan_macos.h>
#endif

namespace kraft
{

static VulkanContext s_Context;

bool VulkanRendererBackend::Init()
{
    s_Context.AllocationCallbacks = nullptr;

    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = "Kraft";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Kraft Engine";
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

    instanceCreateInfo.enabledExtensionCount = arrlen(extensions);
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
        for (int j = 0; j < availableLayers; ++j)
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

    instanceCreateInfo.enabledLayerCount = arrlen(layers);
    instanceCreateInfo.ppEnabledLayerNames = layers;

    KRAFT_VK_CHECK(vkCreateInstance(&instanceCreateInfo, s_Context.AllocationCallbacks, &s_Context.Instance));
    arrfree(extensions);

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

    VulkanPhysicalDeviceRequirements requirements;
    requirements.Compute = true;
    requirements.Graphics = true;
    requirements.Present = true;
    requirements.Transfer = true;
    requirements.DeviceExtensionNames = nullptr;
    arrpush(requirements.DeviceExtensionNames, VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    SelectVulkanPhysicalDevice(&s_Context, requirements);
    CreateVulkanLogicalDevice(&s_Context, requirements);
    CreateVulkanSurface();

    arrfree(requirements.DeviceExtensionNames);
    
    KSUCCESS("[VulkanRendererBackend::Init]: Backend init success!");
    return true;
}

bool VulkanRendererBackend::Shutdown()
{
    DestroyVulkanLogicalDevice(&s_Context);

    if (s_Context.Surface)
    {
        vkDestroySurfaceKHR(s_Context.Instance, s_Context.Surface, s_Context.AllocationCallbacks);
        s_Context.Surface = 0;
    }

    return true;
}

bool VulkanRendererBackend::BeginFrame(float64 deltaTime)
{
    return true;
}

bool VulkanRendererBackend::EndFrame(float64 deltaTime)
{
    return true;
}

void VulkanRendererBackend::OnResize(int width, int height)
{

}

void VulkanRendererBackend::CreateVulkanSurface()
{
#if defined(KRAFT_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    Win32PlatformState* State = (Win32PlatformState*)Platform::InternalState;
    surfaceCreateInfo.hinstance = State->hInstance;
    surfaceCreateInfo.hwnd = State->hWindow;

    KRAFT_VK_CHECK(vkCreateWin32SurfaceKHR(s_Context.Instance, &surfaceCreateInfo, s_Context.AllocationCallbacks, &s_Context.Surface));
#elif defined(KRAFT_PLATFORM_MACOS)
    VkMetalSurfaceCreateInfoKHR surfaceCreateInfo = {VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_KHR};
    MacOSPlatformState* State = (MacOSPlatformState*)Platform::InternalState;

    KRAFT_VK_CHECK(vkCreateMetalSurfaceKHR(s_Context.Instance, &surfaceCreateInfo, s_Context.AllocationCallbacks, &s_Context.Surface));
#elif defined(KRAFT_PLATFORM_LINUX)

#endif
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

}