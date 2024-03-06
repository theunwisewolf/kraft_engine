#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

// TODO: remove this
bool VulkanCreateShaderModule(VulkanContext* context, const char* path, VkShaderModule* out);

bool VulkanCreateShaderModule(VulkanContext* Context, const uint8* Data, uint64 Length, VkShaderModule* Out);
void VulkanDestroyShaderModule(VulkanContext* context, VkShaderModule* out);

}