#pragma once

#include <renderer/vulkan/kraft_vulkan_types.h>

#include <vulkan/vulkan.h>

namespace kraft
{

bool SelectVulkanPhysicalDevice(VulkanContext* context, VulkanPhysicalDeviceRequirements requirements);
    
}