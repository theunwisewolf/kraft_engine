#include "kraft_vulkan_vertex_buffer.h"

#include "core/kraft_memory.h"
#include "renderer/vulkan/kraft_vulkan_buffer.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"

namespace kraft
{

bool VulkanCreateVertexBuffer(VulkanContext* context, const void* data, uint64 size, VulkanBuffer* out)
{
    // Create a staging buffer
    VulkanBuffer stagingBuffer;
    if (!VulkanCreateBuffer(context, size, VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true, &stagingBuffer))
    {
        return false;
    }

    // Create the vertex buffer
    if (!VulkanCreateBuffer(context, size, VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, out))
    {
        return false;
    }

    // Load the given data in the staging buffer
    VulkanLoadDataInBuffer(context, &stagingBuffer, (void*)data, size, 0);

    // Copy the staging buffer data to the vertex buffer
    VulkanCopyBufferTo(context, &stagingBuffer, 0, out, 0, size, context->LogicalDevice.GraphicsQueue, context->LogicalDevice.GraphicsCommandPool);

    // Destroy the staging buffer
    VulkanDestroyBuffer(context, &stagingBuffer);

    return true;
}

}