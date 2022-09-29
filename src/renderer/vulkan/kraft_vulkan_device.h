#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

#include <vulkan/vulkan.h>

namespace kraft
{

VkExtensionProperties* GetAvailableDeviceExtensions(VulkanPhysicalDevice device, uint32* outExtensionCount);
bool DeviceSupportsExtension(VulkanPhysicalDevice device, const char* extension);
bool SelectVulkanPhysicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements requirements);
void CreateVulkanLogicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements requirements);
void DestroyVulkanLogicalDevice(VulkanContext* context);
    
}