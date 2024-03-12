#pragma once

#include "core/kraft_core.h"
#include "core/kraft_log.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

static int32 FindMemoryIndex(VulkanPhysicalDevice device, uint32 typeFilter, uint32 propertyFlags)
{
    for (uint32 i = 0; i < device.MemoryProperties.memoryTypeCount; ++i)
    {
        VkMemoryType type = device.MemoryProperties.memoryTypes[i];
        if (typeFilter & (1 << i) && (type.propertyFlags & propertyFlags) == propertyFlags)
        {
            return i;
        }
    }

    return -1;
}

static bool VulkanAllocateMemory(VulkanContext* context, VkDeviceSize size, int32 typeFilter, VkMemoryPropertyFlags propertyFlags, VkDeviceMemory* out)
{
    int32 memoryTypeIndex = FindMemoryIndex(context->PhysicalDevice, typeFilter, propertyFlags);
    if (memoryTypeIndex == -1)
    {
        KERROR("[VulkanAllocateMemory]: Failed to find suitable memoryType");
        return false;
    }

    VkMemoryAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = size;
    info.memoryTypeIndex = memoryTypeIndex;

    KRAFT_VK_CHECK(vkAllocateMemory(context->LogicalDevice.Handle, &info, context->AllocationCallbacks, out));

    return true;
}

static void VulkanFreeMemory(VulkanContext* context, VkDeviceMemory memory)
{
    vkFreeMemory(context->LogicalDevice.Handle, memory, context->AllocationCallbacks);
}

}