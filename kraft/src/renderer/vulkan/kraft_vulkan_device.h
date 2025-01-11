#pragma once

#include <core/kraft_core.h>

namespace kraft {
struct ArenaAllocator;
}

namespace kraft::renderer {

struct VulkanContext;
struct VulkanSwapchainSupportInfo;
struct VulkanPhysicalDevice;
struct VulkanLogicalDevice;
struct VulkanPhysicalDeviceRequirements;

VkExtensionProperties* VulkanGetAvailableDeviceExtensions(VulkanPhysicalDevice device, uint32* outExtensionCount);
bool                   VulkanDeviceSupportsExtension(VulkanPhysicalDevice device, const char* extension);
bool                   VulkanSelectPhysicalDevice(ArenaAllocator* Arena, VulkanContext* context, VulkanPhysicalDeviceRequirements* Requirements, VulkanPhysicalDevice* out = 0);
void                   VulkanCreateLogicalDevice(ArenaAllocator* Arena, VulkanContext* context, VulkanPhysicalDeviceRequirements* Requirements, VulkanLogicalDevice* out = 0);
void                   VulkanDestroyLogicalDevice(VulkanContext* context);

void VulkanGetSwapchainSupportInfo(ArenaAllocator* Arena, VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo* out);

}