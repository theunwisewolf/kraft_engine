#pragma once

namespace kraft::renderer {

struct VulkanContext;
struct VulkanCommandBuffer;

void VulkanBeginCommandBuffer(VulkanCommandBuffer* buffer, bool singleUse, bool renderPassContinue, bool simultaneousUse);
void VulkanEndCommandBuffer(VulkanCommandBuffer* buffer);
void VulkanSetCommandBufferSubmitted(VulkanCommandBuffer* buffer);
void VulkanResetCommandBuffer(VulkanCommandBuffer* buffer);

// Helpers
// Ends recording on a command buffer, submits it to the
// the given queue and finally frees it up
void VulkanEndAndSubmitSingleUseCommandBuffer(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* buffer, VkQueue queue);

}