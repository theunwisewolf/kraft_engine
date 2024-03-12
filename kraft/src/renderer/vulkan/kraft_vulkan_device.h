#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

#include <vulkan/vulkan.h>

namespace kraft::renderer
{

VkExtensionProperties* VulkanGetAvailableDeviceExtensions(VulkanPhysicalDevice device, uint32* outExtensionCount);
bool VulkanDeviceSupportsExtension(VulkanPhysicalDevice device, const char* extension);
bool VulkanSelectPhysicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements requirements, VulkanPhysicalDevice* out = 0);
void VulkanCreateLogicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements requirements, VulkanLogicalDevice* out = 0);
void VulkanDestroyLogicalDevice(VulkanContext* context);

void VulkanGetSwapchainSupportInfo(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo* out);
    
}