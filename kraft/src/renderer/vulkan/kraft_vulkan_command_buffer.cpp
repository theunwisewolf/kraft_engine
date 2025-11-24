#include <volk/volk.h>

#include "kraft_vulkan_command_buffer.h"

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_memory.h>

#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::r {

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
}

}