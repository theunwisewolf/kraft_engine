#include "kraft_vulkan_image.h"

#include "core/kraft_asserts.h"
#include "renderer/vulkan/kraft_vulkan_memory.h"

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
    VulkanImage* out)
{
    out->Handle = 0;
    out->Memory = 0;
    out->View   = 0;
    out->Width  = width;
    out->Height = height;

    if (tiling == VK_IMAGE_TILING_LINEAR)
    {
        KASSERT(imageType == VK_IMAGE_TYPE_2D);
        // TODO: Depth format restrictions check
    }

    VkImageCreateInfo createInfo = {};
    createInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.pNext         = 0;
    createInfo.imageType     = imageType;
    createInfo.format        = format;
    createInfo.extent.width  = width;
    createInfo.extent.height = height;
    createInfo.extent.depth  = 1;
    createInfo.mipLevels     = 4;
    createInfo.arrayLayers   = 1;
    createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling        = tiling;
    createInfo.usage         = usageFlags;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    KRAFT_VK_CHECK(vkCreateImage(context->LogicalDevice.Handle, &createInfo, context->AllocationCallbacks, &out->Handle));

    // Query memory requirements
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(context->LogicalDevice.Handle, out->Handle, &memoryRequirements);

    // Allocate memory
    if (!VulkanAllocateMemory(context, memoryRequirements.size, memoryRequirements.memoryTypeBits, memoryProperties, &out->Memory))
    {
        return false;
    }

    // Bind memory
    KRAFT_VK_CHECK(vkBindImageMemory(context->LogicalDevice.Handle, out->Handle, out->Memory, 0));

    out->View = 0;
    if (createView)
    {
        VulkanCreateImageView(context, format, imageAspectFlags, out);
    }

    return true;
}
    
void VulkanCreateImageView(
    VulkanContext*      context,
    VkFormat            format,
    VkImageAspectFlags  imageAspectFlags,
    VulkanImage*        image)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image->Handle;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;

    createInfo.subresourceRange.aspectMask = imageAspectFlags;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;

    KRAFT_VK_CHECK(vkCreateImageView(context->LogicalDevice.Handle, &createInfo, context->AllocationCallbacks, &image->View));
}

void VulkanDestroyImage(VulkanContext* context, VulkanImage* image)
{
    if (image->View)
    {
        vkDestroyImageView(context->LogicalDevice.Handle, image->View, context->AllocationCallbacks);
        image->View = 0;
    }

    if (image->Memory)
    {
        VulkanFreeMemory(context, image->Memory);
        image->Memory = 0;
    }

    if (image->Handle)
    {
        vkDestroyImage(context->LogicalDevice.Handle, image->Handle, context->AllocationCallbacks);
        image->Handle = 0;
    }
}

}