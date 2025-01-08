#include "kraft_vulkan_device.h"

#include <core/kraft_core.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>
#include <containers/kraft_carray.h>

namespace kraft::renderer
{

bool checkDepthFormatSupport(VkPhysicalDevice device, VkPhysicalDeviceProperties properties, VulkanPhysicalDeviceRequirements requirements, VkFormat* out)
{
    // No depth buffer required
    if (!requirements.DepthBuffer)
    {
        return true;
    }

    const int candidateCount = 5;
    VkFormat candidates[candidateCount] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };

    for (int i = 0; i < candidateCount; ++i)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device, candidates[i], &properties);
        if (properties.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ||
            properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
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

bool checkQueueSupport(VkPhysicalDevice device, VkPhysicalDeviceProperties properties, VkSurfaceKHR surface, VulkanPhysicalDeviceRequirements requirements, VulkanQueueFamilyInfo* queueFamilyInfo)
{
    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    VkQueueFamilyProperties* queueFamilies = nullptr;
    arrsetlen(queueFamilies, queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

    // We want find dedicated queues for transfer and compute
    int minTransferScore = 0xffff;
    int minComputeScore = 0xffff;
    for (uint32 j = 0; j < queueFamilyCount; ++j)
    {
        VkQueueFamilyProperties queueFamily = queueFamilies[j];
        int transferScore = 0;
        int computeScore = 0;

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            transferScore++;
            computeScore++;
            queueFamilyInfo->GraphicsQueueIndex = j;
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
                queueFamilyInfo->ComputeQueueIndex = j;
                minComputeScore = computeScore;
            }
        }

        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if (transferScore < minTransferScore)
            {
                queueFamilyInfo->TransferQueueIndex = j;
                minTransferScore = transferScore;
            }
        }

        // Check presentation support
        {
            VkBool32 supported = VK_FALSE;
            KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, j, surface, &supported));
            if (supported)
            {
                queueFamilyInfo->PresentQueueIndex = j;
            }
        }
    }

    arrfree(queueFamilies);

    int failures = 0;
    if (requirements.Graphics && queueFamilyInfo->GraphicsQueueIndex == -1)
    {
        KDEBUG("Skipping device %s because graphics queue is not supported and is required", properties.deviceName);
        failures++;
    }

    if (requirements.Compute && queueFamilyInfo->ComputeQueueIndex == -1)
    {
        KDEBUG("Skipping device %s because compute queue is not supported and is required", properties.deviceName);
        failures++;
    }

    if (requirements.Transfer && queueFamilyInfo->TransferQueueIndex == -1)
    {
        KDEBUG("Skipping device %s because transfer queue is not supported and is required", properties.deviceName);
        failures++;
    }

    if (requirements.Present && queueFamilyInfo->PresentQueueIndex == -1)
    {
        KDEBUG("Skipping device %s because no queue family supports presentation", properties.deviceName);
        failures++;
    }

    if (failures > 0) 
    {
        return false;
    }

    return true;
}

bool checkExtensionsSupport(VkPhysicalDevice device, VkPhysicalDeviceProperties properties, VulkanPhysicalDeviceRequirements requirements)
{
    size_t n = arrlen(requirements.DeviceExtensionNames);
    if (n == 0)
    {
        KDEBUG("[checkExtensionsSupport]: No extensions requested", properties.deviceName);
        return true;
    }

    uint32 availableExtensionsCount;
    KRAFT_VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionsCount, nullptr));

    if (availableExtensionsCount == 0)
    {
        KDEBUG("[checkExtensionsSupport]: Skipping device %s because it has no extensions supported and some extensions are required", properties.deviceName);
        return false;
    }

    VkExtensionProperties* availableExtensions = nullptr;
    arrsetlen(availableExtensions, availableExtensionsCount);
    KRAFT_VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionsCount, availableExtensions));

    int failures = 0;

    // Check extensions
    for (int j = 0; j < arrlen(requirements.DeviceExtensionNames); ++j)
    {
        bool found = false;
        const char* extensionName = requirements.DeviceExtensionNames[j];
        for (uint32 k = 0; k < availableExtensionsCount; ++k)
        {
            if (strcmp(extensionName, availableExtensions[k].extensionName) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            KDEBUG("Skipping device %s because required extension %s not available", properties.deviceName, requirements.DeviceExtensionNames[j]);
            failures++;
        }
    }

    arrfree(availableExtensions);
    if (failures > 0)
    {
        return false;
    }

    return true;
}

void VulkanGetSwapchainSupportInfo(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo* out)
{
    MemZero(out, sizeof(VulkanSwapchainSupportInfo));

    // Surface capabilities
    KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &out->SurfaceCapabilities));

    // Suface formats
    if (!out->Formats)
    {
        KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out->FormatCount, 0));
        if (out->FormatCount > 0)
        {
            arrsetlen(out->Formats, out->FormatCount);
            KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &out->FormatCount, out->Formats));
        }
    }

    // Present Modes
    if (!out->PresentModes)
    {
        KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out->PresentModeCount, 0));
        if (out->PresentModeCount > 0)
        {
            arrsetlen(out->PresentModes, out->PresentModeCount);
            KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &out->PresentModeCount, out->PresentModes));
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
    uint32 extensionsCount = 0;
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

bool VulkanSelectPhysicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements requirements, VulkanPhysicalDevice* out)
{
    context->PhysicalDevice = {};

    uint32 deviceCount = 0;
    KRAFT_VK_CHECK(vkEnumeratePhysicalDevices(context->Instance, &deviceCount, nullptr));
    if (deviceCount <= 0)
    {
        KERROR("[VulkanSelectPhysicalDevice]: No vulkan capable physical devices found!");
        return false;
    }

    VkPhysicalDevice* physicalDevices = nullptr;
    arrsetlen(physicalDevices, deviceCount);
    KRAFT_VK_CHECK(vkEnumeratePhysicalDevices(context->Instance, &deviceCount, physicalDevices));

    VulkanPhysicalDevice* SuitablePhysicalDevices;
    arrsetlen(SuitablePhysicalDevices, deviceCount);

    int SuitablePhysicalDevicesCount = 0;
    for (uint32 i = 0; i < deviceCount; ++i)
    {
        VkPhysicalDevice device = physicalDevices[i];

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        KDEBUG("[VulkanSelectPhysicalDevice]: Checking device %s", properties.deviceName);
        if (requirements.DiscreteGPU && properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            KDEBUG("[VulkanSelectPhysicalDevice]: Skipping device %s because device is not a discrete gpu and is required to be one", properties.deviceName);
            continue;
        }

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);

        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        bool supportsDeviceLocalHostVisible = false;
        for (uint32 i = 0; i < memoryProperties.memoryTypeCount; ++i) 
        {
            if (
                ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) &&
                ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)) 
            {
                supportsDeviceLocalHostVisible = true;
                break;
            }
        }

        VulkanQueueFamilyInfo queueFamilyInfo;
        queueFamilyInfo.GraphicsQueueIndex = -1;
        queueFamilyInfo.ComputeQueueIndex  = -1;
        queueFamilyInfo.TransferQueueIndex = -1;

        if (!checkQueueSupport(device, properties, context->Surface, requirements, &queueFamilyInfo))
        {
            continue;
        }

        if (!checkExtensionsSupport(device, properties, requirements))
        {
            continue;
        }

        VkFormat depthBufferFormat = VK_FORMAT_UNDEFINED;
        if (!checkDepthFormatSupport(device, properties, requirements, &depthBufferFormat))
        {
            continue;
        }

        VulkanSwapchainSupportInfo swapchainSupportInfo = {};
        VulkanGetSwapchainSupportInfo(device, context->Surface, &swapchainSupportInfo);
        if (!checkSwapchainSupport(properties, swapchainSupportInfo))
        {
            continue;
        }

        VulkanPhysicalDevice physicalDevice;
        physicalDevice.Handle               = device;
        physicalDevice.Properties           = properties;
        physicalDevice.Features             = features;
        physicalDevice.MemoryProperties     = memoryProperties;
        physicalDevice.QueueFamilyInfo      = queueFamilyInfo;
        physicalDevice.DepthBufferFormat    = depthBufferFormat;
        physicalDevice.SwapchainSupportInfo = swapchainSupportInfo;
        physicalDevice.SupportsDeviceLocalHostVisible = supportsDeviceLocalHostVisible;

        SuitablePhysicalDevices[SuitablePhysicalDevicesCount++] = physicalDevice;
    }

    if (SuitablePhysicalDevicesCount == 0)
    {
        KERROR("[VulkanSelectPhysicalDevice]: Failed to find a suitable physical device!");
        return false;
    }

    // Try to pick a discrete gpu, if possible
    context->PhysicalDevice = SuitablePhysicalDevices[0];
    for (int i = 1; i < SuitablePhysicalDevicesCount; i++)
    {
        if (SuitablePhysicalDevices[i].Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            context->PhysicalDevice = SuitablePhysicalDevices[i];
            break;
        }
    }

    // Print some nice info about our physical device
    KINFO("Selected device: %s [Dedicated GPU = %s]", context->PhysicalDevice.Properties.deviceName, 
        context->PhysicalDevice.Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "true" : "false");

    KINFO("Driver version: %d.%d.%d", 
        VK_VERSION_MAJOR(context->PhysicalDevice.Properties.driverVersion), 
        VK_VERSION_MINOR(context->PhysicalDevice.Properties.driverVersion), 
        VK_VERSION_PATCH(context->PhysicalDevice.Properties.driverVersion));

    KINFO("Vulkan api version: %d.%d.%d", 
        VK_VERSION_MAJOR(context->PhysicalDevice.Properties.apiVersion), 
        VK_VERSION_MINOR(context->PhysicalDevice.Properties.apiVersion), 
        VK_VERSION_PATCH(context->PhysicalDevice.Properties.apiVersion));

    for (uint32 i = 0; i < context->PhysicalDevice.MemoryProperties.memoryHeapCount; i++)
    {
        VkDeviceSize size = context->PhysicalDevice.MemoryProperties.memoryHeaps[i].size;
        float32 sizeInGiB = ((float32)size) / 1024.f / 1024.f / 1024.f;

        if (context->PhysicalDevice.MemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            KINFO("Local GPU memory: %.3f GiB", sizeInGiB);
        }
        else
        {
            KINFO("Shared GPU memory: %.3f GiB", sizeInGiB);
        }
    }

    if (out)    *out = context->PhysicalDevice; 
    arrfree(physicalDevices);

    return true;
}

void VulkanCreateLogicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements requirements, VulkanLogicalDevice* out)
{
    VulkanLogicalDevice device = {};
    device.PhysicalDevice = context->PhysicalDevice;
    context->LogicalDevice = device;

    uint32 indices[3];
    uint32 queueCreateInfoCount = 1; // We need at least 1 queue
    VulkanQueueFamilyInfo familyInfo = context->PhysicalDevice.QueueFamilyInfo;
    indices[0] = familyInfo.GraphicsQueueIndex;

    // First we need to queue create infos for the required queues
    if (familyInfo.GraphicsQueueIndex != familyInfo.ComputeQueueIndex)
    {
        indices[queueCreateInfoCount] = familyInfo.ComputeQueueIndex;
        queueCreateInfoCount++;
    }

    if (familyInfo.ComputeQueueIndex != familyInfo.TransferQueueIndex && 
        familyInfo.GraphicsQueueIndex != familyInfo.TransferQueueIndex)
    {
        indices[queueCreateInfoCount] = familyInfo.TransferQueueIndex;
        queueCreateInfoCount++;
    }

    if (familyInfo.PresentQueueIndex != familyInfo.GraphicsQueueIndex &&
        familyInfo.PresentQueueIndex != familyInfo.ComputeQueueIndex &&
        familyInfo.PresentQueueIndex != familyInfo.TransferQueueIndex)
    {
        indices[queueCreateInfoCount] = familyInfo.PresentQueueIndex;
        queueCreateInfoCount++;
    }

    VkDeviceQueueCreateInfo* createInfos = nullptr;
    float32 queuePriorities = 1.0f;
    arrsetlen(createInfos, queueCreateInfoCount);
    for (uint32 i = 0; i < queueCreateInfoCount; ++i)
    {
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VkDeviceQueueCreateInfo
        createInfos[i].sType = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        createInfos[i].flags = 0;
        createInfos[i].pNext = 0;
        createInfos[i].queueFamilyIndex = indices[i];
        createInfos[i].queueCount = 1;
        createInfos[i].pQueuePriorities = &queuePriorities;
    }

    // Device features to be requested
    // VkPhysicalDeviceFeatures FeatureRequests = 
    // {
    //     .fillModeNonSolid = context->PhysicalDevice.Features.fillModeNonSolid,
    //     .wideLines = context->PhysicalDevice.Features.wideLines,
    // };

    VkPhysicalDeviceFeatures2 FeatureRequests2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    FeatureRequests2.features.fillModeNonSolid = context->PhysicalDevice.Features.fillModeNonSolid;
    FeatureRequests2.features.wideLines = context->PhysicalDevice.Features.wideLines;

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
    if (VulkanDeviceSupportsExtension(context->PhysicalDevice, "VK_KHR_portability_subset"))
    {
        arrpush(requirements.DeviceExtensionNames, "VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo deviceCreateInfo      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount    = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos       = createInfos;
    // deviceCreateInfo.pEnabledFeatures        = &FeatureRequests;
    deviceCreateInfo.enabledExtensionCount   = (uint32)arrlen(requirements.DeviceExtensionNames);
    deviceCreateInfo.ppEnabledExtensionNames = requirements.DeviceExtensionNames;
    deviceCreateInfo.pNext = &FeatureRequests2;

    KRAFT_VK_CHECK(
        vkCreateDevice(context->PhysicalDevice.Handle, 
            &deviceCreateInfo, 
            context->AllocationCallbacks, 
            &context->LogicalDevice.Handle
        )
    );

    arrfree(createInfos);

    volkLoadDevice(context->LogicalDevice.Handle);

    KSUCCESS("[VulkanCreateLogicalDevice]: Successfully created VkDevice");

    // Grab the queues
    vkGetDeviceQueue(context->LogicalDevice.Handle, familyInfo.GraphicsQueueIndex, 0, &context->LogicalDevice.GraphicsQueue);
    vkGetDeviceQueue(context->LogicalDevice.Handle, familyInfo.ComputeQueueIndex,  0, &context->LogicalDevice.ComputeQueue);
    vkGetDeviceQueue(context->LogicalDevice.Handle, familyInfo.TransferQueueIndex, 0, &context->LogicalDevice.TransferQueue);
    vkGetDeviceQueue(context->LogicalDevice.Handle, familyInfo.PresentQueueIndex, 0, &context->LogicalDevice.PresentQueue);
    KDEBUG("[VulkanCreateLogicalDevice]: Required queues obtained");

    if (out)    *out = context->LogicalDevice;
}

void VulkanDestroyLogicalDevice(VulkanContext* context)
{
    context->PhysicalDevice.QueueFamilyInfo = {0};

    vkDestroyDevice(context->LogicalDevice.Handle, context->AllocationCallbacks);
    context->LogicalDevice.Handle  = 0;
    context->PhysicalDevice.Handle = 0;

    KDEBUG("[VulkanDestroyLogicalDevice]: Destroyed VkDevice");
}

}