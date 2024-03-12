#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

// TODO: remove this
bool VulkanCreateShaderModule(VulkanContext* context, const char* path, VkShaderModule* out);

bool VulkanCreateShaderModule(VulkanContext* Context, const char* Data, uint64 Length, VkShaderModule* Out);
void VulkanDestroyShaderModule(VulkanContext* context, VkShaderModule* out);

}