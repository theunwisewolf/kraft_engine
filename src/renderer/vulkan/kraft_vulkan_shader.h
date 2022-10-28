#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

bool VulkanCreateShaderModule(VulkanContext* context, const char* path, VkShaderModule* out);
void VulkanDestroyShaderModule(VulkanContext* context, VkShaderModule* out);

}