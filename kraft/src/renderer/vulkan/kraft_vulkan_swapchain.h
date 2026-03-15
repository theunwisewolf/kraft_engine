#pragma once

#include <core/kraft_core.h>

namespace kraft::r {

struct VulkanContext;
struct VulkanSwapchain;

void VulkanCreateSwapchain(VulkanContext* context, u32 width, u32 height, bool VSync, VulkanSwapchain* out = 0);
void VulkanDestroySwapchain(VulkanContext* context);
void VulkanRecreateSwapchain(VulkanContext* context);
bool VulkanAcquireNextImageIndex(VulkanContext* context, u64 timeoutNS, VkSemaphore imageAvailableSemaphore, VkFence fence, u32* out);

} // namespace kraft::r