#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

void VulkanCreateSwapchain(VulkanContext* context, uint32 width, uint32 height, bool VSync, VulkanSwapchain* out = 0);
void VulkanDestroySwapchain(VulkanContext* context);
void VulkanRecreateSwapchain(VulkanContext* context);
bool VulkanAcquireNextImageIndex(VulkanContext* context, uint64 timeoutNS, VkSemaphore imageAvailableSemaphore, VkFence fence, uint32* out);
void VulkanPresentSwapchain(VulkanContext* context, VkQueue presentQueue, VkSemaphore renderCompleteSemaphore, uint32 presentImageIndex);

}