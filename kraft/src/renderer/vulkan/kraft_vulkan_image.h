#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

struct VulkanImageDescription
{
    VkImageType         Type;
    uint32              Width;
    uint32              Height;
    VkFormat            Format;
    VkImageTiling       Tiling;
    VkImageUsageFlags   UsageFlags;
    VkImageAspectFlags  AspectFlags;
};

bool VulkanCreateImage(
    VulkanContext*          context, 
    VulkanImageDescription  description,
    VkMemoryPropertyFlags   memoryProperties,
    bool                    createView,
    VulkanImage*            out);
    
void VulkanCreateImageView(
    VulkanContext*      context,
    VkFormat            format,
    VkImageAspectFlags  imageAspectFlags,
    VulkanImage*        image);

void VulkanTransitionImageLayout(
    VulkanContext* context,
    VulkanCommandBuffer commandBuffer,
    VulkanImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout);

void VulkanCopyBufferToImage(
    VulkanContext* context,
    VulkanCommandBuffer commandBuffer,
    VulkanBuffer srcBuffer,
    VulkanImage* dstImage
);

void VulkanDestroyImage(VulkanContext* context, VulkanImage* image);

}