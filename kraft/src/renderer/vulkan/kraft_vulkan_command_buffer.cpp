#include "kraft_vulkan_command_buffer.h"

#include "core/kraft_memory.h"

namespace kraft::renderer {

void VulkanAllocateCommandBuffer(VulkanContext* context, VkCommandPool pool, bool primary, VulkanCommandBuffer* out)
{
    MemZero(out, sizeof(VulkanCommandBuffer));

    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = pool;
    allocateInfo.level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocateInfo.commandBufferCount = 1;

    KRAFT_VK_CHECK(vkAllocateCommandBuffers(context->LogicalDevice.Handle, &allocateInfo, &out->Resource));
    out->State = VULKAN_COMMAND_BUFFER_STATE_READY;
}

void VulkanFreeCommandBuffer(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* buffer)
{
    vkFreeCommandBuffers(context->LogicalDevice.Handle, pool, 1, &buffer->Resource);
    buffer->Resource = 0;
    buffer->State = VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED;
}

void VulkanBeginCommandBuffer(VulkanCommandBuffer* buffer, bool singleUse, bool renderPassContinue, bool simultaneousUse)
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = 0;

    if (singleUse)
    {
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }

    if (renderPassContinue)
    {
        info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    if (simultaneousUse)
    {
        info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    }

    KRAFT_VK_CHECK(vkBeginCommandBuffer(buffer->Resource, &info));
    buffer->State = VULKAN_COMMAND_BUFFER_STATE_RECORDING;
}

void VulkanEndCommandBuffer(VulkanCommandBuffer* buffer)
{
    KRAFT_VK_CHECK(vkEndCommandBuffer(buffer->Resource));
    buffer->State = VULKAN_COMMAND_BUFFER_STATE_RECORDING_ENDED;
}

void VulkanSetCommandBufferSubmitted(VulkanCommandBuffer* buffer)
{
    buffer->State = VULKAN_COMMAND_BUFFER_STATE_SUBMITTED;
}

void VulkanResetCommandBuffer(VulkanCommandBuffer* buffer)
{
    buffer->State = VULKAN_COMMAND_BUFFER_STATE_READY;
}

void VulkanAllocateAndBeginSingleUseCommandBuffer(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* out)
{
    // Allocate the command buffer
    VulkanAllocateCommandBuffer(context, pool, true, out);

    // End the command buffer
    VulkanBeginCommandBuffer(out, true, false, false);
}

void VulkanEndAndSubmitSingleUseCommandBuffer(VulkanContext* context, VkCommandPool pool, VulkanCommandBuffer* buffer, VkQueue queue)
{
    // End the command buffer
    VulkanEndCommandBuffer(buffer);

    // Create the submit info
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &buffer->Resource;

    // Submit to the queue
    KRAFT_VK_CHECK(vkQueueSubmit(queue, 1, &info, 0));

    // Wait for it to finish
    KRAFT_VK_CHECK(vkQueueWaitIdle(queue));

    // Free the buffer
    VulkanFreeCommandBuffer(context, pool, buffer);
}

}