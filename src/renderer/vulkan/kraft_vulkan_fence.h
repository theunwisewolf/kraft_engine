#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

void VulkanCreateFence(VulkanContext* context, bool signalled, VulkanFence* out);
void VulkanDestroyFence(VulkanContext* context, VulkanFence* fence);
void VulkanResetFence(VulkanContext* context, VulkanFence* fence);
bool VulkanWaitForFence(VulkanContext* context, VulkanFence* fence, uint64 timeoutNS);

}