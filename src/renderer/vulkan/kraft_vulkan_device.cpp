#include "kraft_vulkan_device.h"

#include "core/kraft_core.h"
#include "core/kraft_asserts.h"
#include "core/kraft_log.h"
#include "containers/array.h"

namespace kraft
{

bool checkQueueSupport(VkPhysicalDevice device, VkPhysicalDeviceProperties properties, VulkanPhysicalDeviceRequirements requirements, VulkanQueueFamilyInfo* queueFamilyInfo)
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

    if (failures > 0) 
    {
        return false;
    }

    return true;
}

bool checkExtensionsSupport(VkPhysicalDevice device, VkPhysicalDeviceProperties properties, VulkanPhysicalDeviceRequirements requirements)
{
    int n = arrlen(requirements.DeviceExtensionNames);
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

bool SelectVulkanPhysicalDevice(VulkanContext* context, kraft::VulkanPhysicalDeviceRequirements requirements)
{
    uint32 deviceCount = 0;
    KRAFT_VK_CHECK(vkEnumeratePhysicalDevices(context->Instance, &deviceCount, nullptr));
    if (deviceCount <= 0)
    {
        KERROR("[SelectVulkanPhysicalDevice]: No vulkan capable physical devices found!");
        return false;
    }

    VkPhysicalDevice* physicalDevices = nullptr;
    arrsetlen(physicalDevices, deviceCount);
    KRAFT_VK_CHECK(vkEnumeratePhysicalDevices(context->Instance, &deviceCount, physicalDevices));

    for (uint32 i = 0; i < deviceCount; ++i)
    {
        VkPhysicalDevice device = physicalDevices[i];

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        KDEBUG("[SelectVulkanPhysicalDevice]: Checking device %s", properties.deviceName);
        if (requirements.DiscreteGPU && properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            KDEBUG("[SelectVulkanPhysicalDevice]: Skipping device %s because device is not a discrete gpu and is required to be one", properties.deviceName);
            continue;
        }

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);

        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        VulkanQueueFamilyInfo queueFamilyInfo;
        if (!checkQueueSupport(device, properties, requirements, &queueFamilyInfo))
        {
            continue;
        }

        if (!checkExtensionsSupport(device, properties, requirements))
        {
            continue;
        }

        VulkanPhysicalDevice physicalDevice;
        physicalDevice.Device           = device;
        physicalDevice.Properties       = properties;
        physicalDevice.Features         = features;
        physicalDevice.MemoryProperties = memoryProperties;
        physicalDevice.QueueFamilyInfo  = queueFamilyInfo;

        context->PhysicalDevice = physicalDevice;
        break;
    }

    if (!context->PhysicalDevice.Device)
    {
        KERROR("[SelectVulkanPhysicalDevice]: Failed to find a suitable physical device!");
        return false;
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

    for (int i = 0; i < context->PhysicalDevice.MemoryProperties.memoryHeapCount; i++)
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
}

}