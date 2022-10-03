#pragma once

#include "core/kraft_core.h"
#include "core/kraft_asserts.h"

#include <vulkan/vulkan.h>

#define KRAFT_VK_CHECK(expression) \
    KRAFT_ASSERT(expression == VK_SUCCESS)

namespace kraft
{

struct VulkanQueueFamilyInfo
{
    uint32 GraphicsQueueIndex;
    uint32 ComputeQueueIndex;
    uint32 TransferQueueIndex;
    uint32 PresentQueueIndex;
};

struct VulkanPhysicalDevice
{
    VkPhysicalDevice                    Device;
    VkPhysicalDeviceProperties          Properties; 
    VkPhysicalDeviceFeatures            Features;
    VkPhysicalDeviceMemoryProperties    MemoryProperties;
    VulkanQueueFamilyInfo               QueueFamilyInfo;
};

struct VulkanLogicalDevice
{
    VulkanPhysicalDevice    PhysicalDevice;
    VkDevice                Device;
    VkQueue                 GraphicsQueue;
    VkQueue                 ComputeQueue;
    VkQueue                 TransferQueue;
    VkQueue                 PresentQueue;
};

struct VulkanContext
{
    VkInstance              Instance;
    VkAllocationCallbacks*  AllocationCallbacks;
    VulkanPhysicalDevice    PhysicalDevice;
    VulkanLogicalDevice     LogicalDevice;
    VkSurfaceKHR            Surface;

#ifdef KRAFT_DEBUG
    VkDebugUtilsMessengerEXT DebugMessenger;
#endif
};

struct VulkanPhysicalDeviceRequirements
{
    bool Graphics;
    bool Present;
    bool Transfer;
    bool Compute;
    bool DiscreteGPU;

    const char** DeviceExtensionNames;
};

}