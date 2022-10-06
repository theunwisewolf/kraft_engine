#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

bool VulkanCreateImage(
    VulkanContext*      context, 
    VkImageType         imageType, 
    uint32              width, 
    uint32              height, 
    VkFormat            format, 
    VkImageTiling       tiling, 
    VkImageUsageFlags   usageFlags,
    VkMemoryPropertyFlags memoryProperties,
    bool                createView,
    VkImageAspectFlags  imageAspectFlags,
    VulkanImage* out);
    
void VulkanCreateImageView(
    VulkanContext*      context,
    VkFormat            format,
    VkImageAspectFlags  imageAspectFlags,
    VulkanImage*        image);

void VulkanDestroyImage(VulkanContext* context, VulkanImage* image);

}