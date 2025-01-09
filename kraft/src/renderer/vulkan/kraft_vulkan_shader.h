#pragma once

#include <core/kraft_core.h>

namespace kraft::renderer {

struct VulkanContext;

// TODO: remove this
bool VulkanCreateShaderModule(VulkanContext* context, const char* path, VkShaderModule* out);

bool VulkanCreateShaderModule(VulkanContext* Context, const char* Data, uint64 Length, VkShaderModule* Out);
void VulkanDestroyShaderModule(VulkanContext* context, VkShaderModule* out);

}