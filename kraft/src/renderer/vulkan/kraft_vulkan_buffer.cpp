#include "kraft_vulkan_buffer.h"

#include "core/kraft_memory.h"
#include "renderer/vulkan/kraft_vulkan_memory.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"

namespace kraft
{

bool VulkanCreateBuffer(VulkanContext* context, uint64 size, VkSharingMode sharingMode, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryProperties, bool bind, VulkanBuffer* out)
{
    MemZero(out, sizeof(*out));

    out->Size = size;
    out->UsageFlags = usageFlags;
    out->MemoryPropertyFlags = memoryProperties;

    VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    createInfo.size = size;
    createInfo.sharingMode = sharingMode;
    createInfo.usage = usageFlags;
    
    KRAFT_VK_CHECK(vkCreateBuffer(context->LogicalDevice.Handle, &createInfo, context->AllocationCallbacks, &out->Handle));

    // Query memory requirements
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(context->LogicalDevice.Handle, out->Handle, &memoryRequirements);

    // Allocate memory
    if (!VulkanAllocateMemory(context, memoryRequirements.size, memoryRequirements.memoryTypeBits, memoryProperties, &out->Memory))
    {
        return false;
    }

    if (bind)
    {
        VulkanBindBufferMemory(context, out, 0);
    }

    return true;
}

void VulkanBindBufferMemory(VulkanContext* context, VulkanBuffer* buffer, uint64 offset)
{
    KRAFT_VK_CHECK(vkBindBufferMemory(context->LogicalDevice.Handle, buffer->Handle, buffer->Memory, offset));
}

void* VulkanMapBufferMemory(VulkanContext* context, VulkanBuffer* buffer, uint64 size, uint64 offset)
{
    void *data;
    KRAFT_VK_CHECK(vkMapMemory(context->LogicalDevice.Handle, buffer->Memory, offset, size, 0, &data));

    return data;
}

void VulkanLoadDataInBuffer(VulkanContext* context, VulkanBuffer* buffer, void* in, uint64 size, uint64 offset)
{
    void *out = VulkanMapBufferMemory(context, buffer, size, offset);
    MemCpy(out, in, size);
    VulkanUnmapBufferMemory(context, buffer);
}

void VulkanUnmapBufferMemory(VulkanContext* context, VulkanBuffer* buffer)
{
    vkUnmapMemory(context->LogicalDevice.Handle, buffer->Memory);
}

void VulkanCopyBufferTo(VulkanContext* context, VulkanBuffer* src, uint64 srcOffset, VulkanBuffer* dst, uint64 dstOffset, uint64 size, VkQueue queue, VkCommandPool commandPool)
{
    vkDeviceWaitIdle(context->LogicalDevice.Handle);

    VulkanCommandBuffer commandBuffer;
    VulkanAllocateAndBeginSingleUseCommandBuffer(context, commandPool, &commandBuffer);

    VkBufferCopy bufferCopyInfo = {};
    bufferCopyInfo.size = size;
    bufferCopyInfo.srcOffset = srcOffset;
    bufferCopyInfo.dstOffset = dstOffset;

    vkCmdCopyBuffer(commandBuffer.Handle, src->Handle, dst->Handle, 1, &bufferCopyInfo);
    VulkanEndAndSubmitSingleUseCommandBuffer(context, commandPool, &commandBuffer, queue);
}

bool VulkanResizeBuffer(VulkanContext* context, VulkanBuffer* buffer, uint64 size, VkQueue queue, VkCommandPool commandPool)
{
    VulkanBuffer newBuffer;
    
    if (!VulkanCreateBuffer(context, size, VK_SHARING_MODE_EXCLUSIVE, buffer->UsageFlags, buffer->MemoryPropertyFlags, true, &newBuffer))
    {
        return false;
    }

    VulkanCopyBufferTo(context, buffer, 0, &newBuffer, 0, size, queue, commandPool);
    
    vkDeviceWaitIdle(context->LogicalDevice.Handle);
    VulkanDestroyBuffer(context, buffer);

    *buffer = newBuffer;

    return true;
}

void VulkanDestroyBuffer(VulkanContext* context, VulkanBuffer* buffer)
{
    if (!buffer) return;

    if (buffer->Memory)
    {
        vkFreeMemory(context->LogicalDevice.Handle, buffer->Memory, context->AllocationCallbacks);
        buffer->Memory = 0;
    }

    if (buffer->Handle)
    {
        vkDestroyBuffer(context->LogicalDevice.Handle, buffer->Handle, context->AllocationCallbacks);
        buffer->Handle = 0;
    }

    buffer->IsLocked = false;
    buffer->MemoryIndex = 0;
    buffer->MemoryPropertyFlags = 0;
    buffer->Size = 0;
}

}