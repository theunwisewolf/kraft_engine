#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer {

void VulkanAllocateCommandBuffer(VulkanContext* context, VkCommandPool pool, bool primary, VulkanCommandBuffer* out);
void VulkanFreeCommandBuffer(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* buffer);
void VulkanBeginCommandBuffer(VulkanCommandBuffer* buffer, bool singleUse, bool renderPassContinue, bool simultaneousUse);
void VulkanEndCommandBuffer(VulkanCommandBuffer* buffer);
void VulkanSetCommandBufferSubmitted(VulkanCommandBuffer* buffer);
void VulkanResetCommandBuffer(VulkanCommandBuffer* buffer);

// Helpers
// Allocates a command buffer on the given command pool
// and begins recording
void VulkanAllocateAndBeginSingleUseCommandBuffer(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* out);

// Ends recording on a command buffer, submits it to the
// the given queue and finally frees it up
void VulkanEndAndSubmitSingleUseCommandBuffer(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* buffer, VkQueue queue);

}