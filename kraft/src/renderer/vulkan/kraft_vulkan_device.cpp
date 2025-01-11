#include <volk/volk.h>

#include "kraft_vulkan_device.h"

#include <containers/kraft_array.h>
#include <containers/kraft_carray.h>
#include <core/kraft_allocators.h>
#include <core/kraft_asserts.h>
#include <core/kraft_core.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>

#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::renderer {

bool checkDepthFormatSupport(VkPhysicalDevice device, VkPhysicalDeviceProperties properties, VulkanPhysicalDeviceRequirements* requirements, VkFormat* out)
{
    // No depth buffer required
    if (!requirements->DepthBuffer)
    {
        return true;
    }

    const int candidateCount = 5;
    VkFormat  candidates[candidateCount] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };

    for (int i = 0; i < candidateCount; ++i)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device, candidates[i], &properties);
        if (properties.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT || properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            *out = candidates[i];
            return true;
        }
    }

    *out = VK_FORMAT_UNDEFINED;
    KDEBUG("Skipping device %s because device does not support the required depth buffer format", properties.deviceName);
    return false;
}

bool checkSwapchainSupport(VkPhysicalDeviceProperties properties, VulkanSwapchainSupportInfo info)
{
    if (info.FormatCount <= 0)
    {
        KDEBUG("Skipping device %s because swapchain has no formats", properties.deviceName);
        return false;
    }

    if (info.PresentModeCount <= 0)
    {
        KDEBUG("Skipping device %s because swapchain has no present modes", properties.deviceName);
        return false;
    }

    return true;
}

bool checkQueueSupport(
    ArenaAllocator*                   Arena,
    VkPhysicalDevice                  PhysicalDevice,
    VkPhysicalDeviceProperties        Properties,
    VkSurfaceKHR                      Surface,
    VulkanPhysicalDeviceRequirements* Requirements,
    VulkanQueueFamilyInfo*            QueueFamilyInfo
)
{
    uint32 QueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);

    VkQueueFamilyProperties* QueueFamilies = ArenaPushCArray<VkQueueFamilyProperties>(Arena, QueueFamilyCount, true);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilies);

    // We want find dedicated queues for transfer and compute
    int minTransferScore = 0xffff;
    int minComputeScore = 0xffff;
    for (uint32 j = 0; j < QueueFamilyCount; ++j)
    {
        VkQueueFamilyProperties queueFamily = QueueFamilies[j];
        int                     transferScore = 0;
        int                     computeScore = 0;

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            transferScore++;
            computeScore++;
            QueueFamilyInfo->GraphicsQueueIndex = j;
        }

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            transferScore++;

            // This queue also supports transfer
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                computeScore++;
            }

            if (computeScore < minComputeScore)
            {
                QueueFamilyInfo->ComputeQueueIndex = j;
                minComputeScore = computeScore;
            }
        }

        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if (transferScore < minTransferScore)
            {
                QueueFamilyInfo->TransferQueueIndex = j;
                minTransferScore = transferScore;
            }
        }

        // Check presentation support
        {
            VkBool32 supported = VK_FALSE;
            KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, j, Surface, &supported));
            if (supported)
            {
                QueueFamilyInfo->PresentQueueIndex = j;
            }
        }
    }

    ArenaPopCArray(Arena, QueueFamilies, QueueFamilyCount);

    uint32 UnsupportedQueueCount = 0;
    if (Requirements->Graphics && QueueFamilyInfo->GraphicsQueueIndex == -1)
    {
        KDEBUG("Device '%s' does not support a 'graphics' queue.", Properties.deviceName);
        UnsupportedQueueCount++;
    }

    if (Requirements->Compute && QueueFamilyInfo->ComputeQueueIndex == -1)
    {
        KDEBUG("Device '%s' does not support a 'compute' queue.", Properties.deviceName);
        UnsupportedQueueCount++;
    }

    if (Requirements->Transfer && QueueFamilyInfo->TransferQueueIndex == -1)
    {
        KDEBUG("Device '%s' does not support a 'transfer' queue.", Properties.deviceName);
        UnsupportedQueueCount++;
    }

    if (Requirements->Present && QueueFamilyInfo->PresentQueueIndex == -1)
    {
        KDEBUG("Device '%s' does not support a 'present' queue.", Properties.deviceName);
        UnsupportedQueueCount++;
    }

    if (UnsupportedQueueCount > 0)
    {
        KDEBUG("Skipping device %s because one or more required queue families are unsupported.", Properties.deviceName);
        return false;
    }

    return true;
}

bool checkExtensionsSupport(ArenaAllocator* Arena, VkPhysicalDevice device, VkPhysicalDeviceProperties properties, VulkanPhysicalDeviceRequirements* Requirements)
{
    if (Requirements->DeviceExtensionsCount == 0)
        return true;

    uint32 AvailableExtensionsCount = 0;
    KRAFT_VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &AvailableExtensionsCount, nullptr));

    if (AvailableExtensionsCount == 0)
    {
        KDEBUG("[checkExtensionsSupport]: Skipping device %s because it has no extensions supported and some extensions are required", properties.deviceName);
        return false;
    }

    VkExtensionProperties* AvailableExtensions = ArenaPushCArray<VkExtensionProperties>(Arena, AvailableExtensionsCount, true);
    KRAFT_VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &AvailableExtensionsCount, AvailableExtensions));

    int MissingExtensionsCount = 0;

    // Check extensions
    for (int j = 0; j < Requirements->DeviceExtensionsCount; ++j)
    {
        bool        found = false;
        const char* extensionName = Requirements->DeviceExtensions[j];
        for (uint32 k = 0; k < AvailableExtensionsCount; ++k)
        {
            if (strcmp(extensionName, AvailableExtensions[k].extensionName) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            KINFO("Device %s does not support a required extension %s", properties.deviceName, Requirements->DeviceExtensions[j]);
            MissingExtensionsCount++;
        }
    }

    ArenaPopCArray(Arena, AvailableExtensions, AvailableExtensionsCount);
    if (MissingExtensionsCount > 0)
    {
        KINFO("Device %s skipped", properties.deviceName);
        return false;
    }

    return true;
}

void VulkanGetSwapchainSupportInfo(ArenaAllocator* Arena, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VulkanSwapchainSupportInfo* Out)
{
    MemZero(Out, sizeof(VulkanSwapchainSupportInfo));

    // Surface capabilities
    KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &Out->SurfaceCapabilities));

    // Suface formats
    if (!Out->Formats)
    {
        KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &Out->FormatCount, 0));
        if (Out->FormatCount > 0)
        {
            Out->Formats = ArenaPushCArray<VkSurfaceFormatKHR>(Arena, Out->FormatCount, true);
            KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &Out->FormatCount, Out->Formats));
        }
    }

    // Present Modes
    if (!Out->PresentModes)
    {
        KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &Out->PresentModeCount, 0));
        if (Out->PresentModeCount > 0)
        {
            Out->PresentModes = ArenaPushCArray<VkPresentModeKHR>(Arena, Out->PresentModeCount, true);
            KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &Out->PresentModeCount, Out->PresentModes));
        }
    }
}

VkExtensionProperties* VulkanGetAvailableDeviceExtensions(VulkanPhysicalDevice device, uint32* outExtensionCount)
{
    KRAFT_VK_CHECK(vkEnumerateDeviceExtensionProperties(device.Handle, nullptr, outExtensionCount, nullptr));
    if (outExtensionCount == 0)
    {
        return nullptr;
    }

    VkExtensionProperties* outExtensionProperties = nullptr;
    arrsetlen(outExtensionProperties, *outExtensionCount);
    KRAFT_VK_CHECK(vkEnumerateDeviceExtensionProperties(device.Handle, nullptr, outExtensionCount, outExtensionProperties));

    return outExtensionProperties;
}

bool VulkanDeviceSupportsExtension(VulkanPhysicalDevice device, const char* extension)
{
    uint32                 extensionsCount = 0;
    VkExtensionProperties* availableExtensions = VulkanGetAvailableDeviceExtensions(device, &extensionsCount);

    for (uint32 k = 0; k < extensionsCount; ++k)
    {
        if (strcmp(extension, availableExtensions[k].extensionName) == 0)
        {
            return true;
        }
    }

    arrfree(availableExtensions);
    return false;
}

bool VulkanSelectPhysicalDevice(ArenaAllocator* Arena, VulkanContext* Context, VulkanPhysicalDeviceRequirements* Requirements, VulkanPhysicalDevice* out)
{
    Context->PhysicalDevice = {};

    uint32 DeviceCount = 0;
    KRAFT_VK_CHECK(vkEnumeratePhysicalDevices(Context->Instance, &DeviceCount, nullptr));
    if (DeviceCount <= 0)
    {
        KERROR("[VulkanSelectPhysicalDevice]: No vulkan capable physical devices found!");
        return false;
    }

    VkPhysicalDevice* PhysicalDevices = ArenaPushCArray<VkPhysicalDevice>(Arena, DeviceCount, true);
    KRAFT_VK_CHECK(vkEnumeratePhysicalDevices(Context->Instance, &DeviceCount, PhysicalDevices));

    VulkanPhysicalDevice* SuitablePhysicalDevices = ArenaPushCArray<VulkanPhysicalDevice>(Arena, DeviceCount, true);

    int SuitablePhysicalDevicesCount = 0;
    for (uint32 i = 0; i < DeviceCount; ++i)
    {
        VkPhysicalDevice           PhysicalDevice = PhysicalDevices[i];
        VkPhysicalDeviceProperties Properties = {};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);

        KDEBUG("[VulkanSelectPhysicalDevice]: Checking device %s", Properties.deviceName);
        if (Requirements->DiscreteGPU && Properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            KDEBUG("[VulkanSelectPhysicalDevice]: Skipping device %s because device is not a discrete gpu and is required to be one", Properties.deviceName);
            continue;
        }

        VkPhysicalDeviceFeatures Features = {};
        vkGetPhysicalDeviceFeatures(PhysicalDevice, &Features);

        VkPhysicalDeviceMemoryProperties MemoryProperties = {};
        vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

        bool SupportsDeviceLocalHostVisible = false;
        for (uint32 i = 0; i < MemoryProperties.memoryTypeCount; ++i)
        {
            if (((MemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) &&
                ((MemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0))
            {
                SupportsDeviceLocalHostVisible = true;
                break;
            }
        }

        VulkanQueueFamilyInfo QueueFamilyInfo = { -1 };
        QueueFamilyInfo.GraphicsQueueIndex = -1;
        QueueFamilyInfo.ComputeQueueIndex = -1;
        QueueFamilyInfo.TransferQueueIndex = -1;

        if (!checkQueueSupport(Arena, PhysicalDevice, Properties, Context->Surface, Requirements, &QueueFamilyInfo))
        {
            continue;
        }

        if (!checkExtensionsSupport(Arena, PhysicalDevice, Properties, Requirements))
        {
            continue;
        }

        VkFormat DepthBufferFormat = VK_FORMAT_UNDEFINED;
        if (!checkDepthFormatSupport(PhysicalDevice, Properties, Requirements, &DepthBufferFormat))
        {
            continue;
        }

        VulkanSwapchainSupportInfo SwapchainSupportInfo = {};
        VulkanGetSwapchainSupportInfo(Arena, PhysicalDevice, Context->Surface, &SwapchainSupportInfo);
        if (!checkSwapchainSupport(Properties, SwapchainSupportInfo))
        {
            continue;
        }

        VulkanPhysicalDevice OutPhysicalDevice = {};
        OutPhysicalDevice.Handle = PhysicalDevice;
        OutPhysicalDevice.Properties = Properties;
        OutPhysicalDevice.Features = Features;
        OutPhysicalDevice.MemoryProperties = MemoryProperties;
        OutPhysicalDevice.QueueFamilyInfo = QueueFamilyInfo;
        OutPhysicalDevice.DepthBufferFormat = DepthBufferFormat;
        OutPhysicalDevice.SwapchainSupportInfo = SwapchainSupportInfo;
        OutPhysicalDevice.SupportsDeviceLocalHostVisible = SupportsDeviceLocalHostVisible;

        SuitablePhysicalDevices[SuitablePhysicalDevicesCount++] = OutPhysicalDevice;
    }

    if (SuitablePhysicalDevicesCount == 0)
    {
        KERROR("[VulkanSelectPhysicalDevice]: Failed to find a suitable physical device!");
        return false;
    }

    // Try to pick a discrete gpu, if possible
    Context->PhysicalDevice = SuitablePhysicalDevices[0];
    for (int i = 1; i < SuitablePhysicalDevicesCount; i++)
    {
        if (SuitablePhysicalDevices[i].Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            Context->PhysicalDevice = SuitablePhysicalDevices[i];
            break;
        }
    }

    // Print some nice info about our physical device
    KINFO(
        "Selected device: %s [Dedicated GPU = %s]",
        Context->PhysicalDevice.Properties.deviceName,
        Context->PhysicalDevice.Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "true" : "false"
    );

    KINFO(
        "Driver version: %d.%d.%d",
        VK_VERSION_MAJOR(Context->PhysicalDevice.Properties.driverVersion),
        VK_VERSION_MINOR(Context->PhysicalDevice.Properties.driverVersion),
        VK_VERSION_PATCH(Context->PhysicalDevice.Properties.driverVersion)
    );

    KINFO(
        "Vulkan API version: %d.%d.%d",
        VK_VERSION_MAJOR(Context->PhysicalDevice.Properties.apiVersion),
        VK_VERSION_MINOR(Context->PhysicalDevice.Properties.apiVersion),
        VK_VERSION_PATCH(Context->PhysicalDevice.Properties.apiVersion)
    );

    for (uint32 i = 0; i < Context->PhysicalDevice.MemoryProperties.memoryHeapCount; i++)
    {
        VkDeviceSize size = Context->PhysicalDevice.MemoryProperties.memoryHeaps[i].size;
        float32      sizeInGiB = ((float32)size) / 1024.f / 1024.f / 1024.f;

        if (Context->PhysicalDevice.MemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            KINFO("Local GPU memory: %.3f GiB", sizeInGiB);
        }
        else
        {
            KINFO("Shared GPU memory: %.3f GiB", sizeInGiB);
        }
    }

    if (out)
        *out = Context->PhysicalDevice;

    ArenaPopCArray(Arena, PhysicalDevices, DeviceCount);
    ArenaPopCArray(Arena, SuitablePhysicalDevices, DeviceCount);

    return true;
}

void VulkanCreateLogicalDevice(ArenaAllocator* Arena, VulkanContext* Context, VulkanPhysicalDeviceRequirements* Requirements, VulkanLogicalDevice* Out)
{
    VulkanLogicalDevice LogicalDevice = {};
    LogicalDevice.PhysicalDevice = Context->PhysicalDevice;
    Context->LogicalDevice = LogicalDevice;

    uint32                Indices[3] = {0};
    uint32                QueueCreateInfoCount = 1; // We need at least 1 queue
    VulkanQueueFamilyInfo FamilyInfo = Context->PhysicalDevice.QueueFamilyInfo;
    Indices[0] = FamilyInfo.GraphicsQueueIndex;

    // First we need to queue create infos for the required queues
    if (FamilyInfo.GraphicsQueueIndex != FamilyInfo.ComputeQueueIndex)
    {
        Indices[QueueCreateInfoCount] = FamilyInfo.ComputeQueueIndex;
        QueueCreateInfoCount++;
    }

    if (FamilyInfo.ComputeQueueIndex != FamilyInfo.TransferQueueIndex && FamilyInfo.GraphicsQueueIndex != FamilyInfo.TransferQueueIndex)
    {
        Indices[QueueCreateInfoCount] = FamilyInfo.TransferQueueIndex;
        QueueCreateInfoCount++;
    }

    if (FamilyInfo.PresentQueueIndex != FamilyInfo.GraphicsQueueIndex && FamilyInfo.PresentQueueIndex != FamilyInfo.ComputeQueueIndex && FamilyInfo.PresentQueueIndex != FamilyInfo.TransferQueueIndex)
    {
        Indices[QueueCreateInfoCount] = FamilyInfo.PresentQueueIndex;
        QueueCreateInfoCount++;
    }

    VkDeviceQueueCreateInfo* QueueCreateInfos = ArenaPushCArray<VkDeviceQueueCreateInfo>(Arena, QueueCreateInfoCount, true);
    float32                  QueuePriorities = 1.0f;

    for (uint32 i = 0; i < QueueCreateInfoCount; ++i)
    {
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VkDeviceQueueCreateInfo
        QueueCreateInfos[i].sType = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        QueueCreateInfos[i].flags = 0;
        QueueCreateInfos[i].pNext = 0;
        QueueCreateInfos[i].queueFamilyIndex = Indices[i];
        QueueCreateInfos[i].queueCount = 1;
        QueueCreateInfos[i].pQueuePriorities = &QueuePriorities;
    }

    // Device features to be requested
    // VkPhysicalDeviceFeatures FeatureRequests =
    // {
    //     .fillModeNonSolid = context->PhysicalDevice.Features.fillModeNonSolid,
    //     .wideLines = context->PhysicalDevice.Features.wideLines,
    // };

    VkPhysicalDeviceFeatures2 FeatureRequests2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    FeatureRequests2.features.fillModeNonSolid = Context->PhysicalDevice.Features.fillModeNonSolid;
    FeatureRequests2.features.wideLines = Context->PhysicalDevice.Features.wideLines;

    VkPhysicalDeviceVulkan11Features FeatureRequests11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    VkPhysicalDeviceVulkan12Features FeatureRequests12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan13Features FeatureRequests13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    FeatureRequests13.dynamicRendering = true;
    FeatureRequests13.synchronization2 = true;

    // VkPhysicalDeviceSynchronization2Features SyncFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
    // SyncFeatures.synchronization2 = true;

    FeatureRequests2.pNext = &FeatureRequests11;
    FeatureRequests11.pNext = &FeatureRequests12;
    FeatureRequests12.pNext = &FeatureRequests13;
    // FeatureRequests13.pNext = &SyncFeatures;

    // As per the vulkan spec, if the device supports VK_KHR_portability_subset
    // then the extension must be included
    if (VulkanDeviceSupportsExtension(Context->PhysicalDevice, "VK_KHR_portability_subset"))
    {
        Requirements->DeviceExtensions[Requirements->DeviceExtensionsCount] = "VK_KHR_portability_subset";
        Requirements->DeviceExtensionsCount++;
    }

    VkDeviceCreateInfo DeviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    DeviceCreateInfo.queueCreateInfoCount = QueueCreateInfoCount;
    DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos;
    // DeviceCreateInfo.pEnabledFeatures        = &FeatureRequests;
    DeviceCreateInfo.enabledExtensionCount = Requirements->DeviceExtensionsCount;
    DeviceCreateInfo.ppEnabledExtensionNames = Requirements->DeviceExtensions;
    DeviceCreateInfo.pNext = &FeatureRequests2;

    KRAFT_VK_CHECK(vkCreateDevice(Context->PhysicalDevice.Handle, &DeviceCreateInfo, Context->AllocationCallbacks, &Context->LogicalDevice.Handle));

    ArenaPopCArray(Arena, QueueCreateInfos, QueueCreateInfoCount);

    volkLoadDevice(Context->LogicalDevice.Handle);

    KSUCCESS("[VulkanCreateLogicalDevice]: Successfully created VkDevice");

    // Grab the queues
    vkGetDeviceQueue(Context->LogicalDevice.Handle, FamilyInfo.GraphicsQueueIndex, 0, &Context->LogicalDevice.GraphicsQueue);
    vkGetDeviceQueue(Context->LogicalDevice.Handle, FamilyInfo.ComputeQueueIndex, 0, &Context->LogicalDevice.ComputeQueue);
    vkGetDeviceQueue(Context->LogicalDevice.Handle, FamilyInfo.TransferQueueIndex, 0, &Context->LogicalDevice.TransferQueue);
    vkGetDeviceQueue(Context->LogicalDevice.Handle, FamilyInfo.PresentQueueIndex, 0, &Context->LogicalDevice.PresentQueue);
    KDEBUG("[VulkanCreateLogicalDevice]: Required queues obtained");

    if (Out)
        *Out = Context->LogicalDevice;
}

void VulkanDestroyLogicalDevice(VulkanContext* Context)
{
    Context->PhysicalDevice.QueueFamilyInfo = { 0 };

    vkDestroyDevice(Context->LogicalDevice.Handle, Context->AllocationCallbacks);
    Context->LogicalDevice.Handle = 0;
    Context->PhysicalDevice.Handle = 0;

    KDEBUG("[VulkanDestroyLogicalDevice]: Destroyed VkDevice");
}

}