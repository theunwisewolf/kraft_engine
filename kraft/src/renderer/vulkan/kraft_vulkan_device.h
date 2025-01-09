#pragma once

#include <core/kraft_core.h>

namespace kraft::renderer {

struct VulkanContext;
struct VulkanSwapchainSupportInfo;
struct VulkanPhysicalDevice;
struct VulkanLogicalDevice;
struct VulkanPhysicalDeviceRequirements;

VkExtensionProperties* VulkanGetAvailableDeviceExtensions(VulkanPhysicalDevice device, uint32* outExtensionCount);
bool                   VulkanDeviceSupportsExtension(VulkanPhysicalDevice device, const char* extension);
bool                   VulkanSelectPhysicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements* Requirements, VulkanPhysicalDevice* out = 0);
void                   VulkanCreateLogicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements* Requirements, VulkanLogicalDevice* out = 0);
void                   VulkanDestroyLogicalDevice(VulkanContext* context);

void VulkanGetSwapchainSupportInfo(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo* out);

}