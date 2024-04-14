#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

bool VulkanCreateBuffer(VulkanContext* context, uint64 size, VkSharingMode sharingMode, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryProperties, bool bind, VulkanBuffer* out);
void VulkanBindBufferMemory(VulkanContext* context, VulkanBuffer* buffer, uint64 offset);
void* VulkanMapBufferMemory(VulkanContext* context, VulkanBuffer* buffer, uint64 size, uint64 offset);
void VulkanLoadDataInBuffer(VulkanContext* context, VulkanBuffer* buffer, const void* data, uint64 size, uint64 offset);
void VulkanUnmapBufferMemory(VulkanContext* context, VulkanBuffer* buffer);
void VulkanCopyBufferTo(VulkanContext* context, VulkanBuffer* src, uint64 srcOffset, VulkanBuffer* dst, uint64 dstOffset, uint64 size, VkQueue queue, VkCommandPool commandPool);
bool VulkanResizeBuffer(VulkanContext* context, VulkanBuffer* buffer, uint64 size, VkQueue queue, VkCommandPool commandPool);
void VulkanDestroyBuffer(VulkanContext* context, VulkanBuffer* buffer);

}