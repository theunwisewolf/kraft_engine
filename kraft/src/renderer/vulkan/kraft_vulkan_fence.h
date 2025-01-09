#pragma once

#include <core/kraft_core.h>

namespace kraft::renderer {

struct VulkanContext;
struct VulkanFence;

void VulkanCreateFence(VulkanContext* context, bool signalled, VulkanFence* out);
void VulkanDestroyFence(VulkanContext* context, VulkanFence* fence);
void VulkanResetFence(VulkanContext* context, VulkanFence* fence);
bool VulkanWaitForFence(VulkanContext* context, VulkanFence* fence, uint64 timeoutNS);

}