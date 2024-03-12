#include "kraft_vulkan_image.h"

#include "core/kraft_asserts.h"
#include "renderer/vulkan/kraft_vulkan_memory.h"

namespace kraft::renderer
{

bool VulkanCreateImage(
    VulkanContext*          context, 
    VulkanImageDescription  description,
    VkMemoryPropertyFlags   memoryProperties,
    bool                    createView,
    VulkanImage*            out)
{
    out->View   = 0;
    out->Handle = 0;
    out->Memory = 0;
    out->View   = 0;
    out->Width  = description.Width;
    out->Height = description.Height;

    if (description.Tiling == VK_IMAGE_TILING_LINEAR)
    {
        KASSERT(description.Type == VK_IMAGE_TYPE_2D);
        // TODO: Depth format restrictions check
    }

    VkImageCreateInfo createInfo = {};
    createInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.pNext         = 0;
    createInfo.imageType     = description.Type;
    createInfo.format        = description.Format;
    createInfo.extent.width  = description.Width;
    createInfo.extent.height = description.Height;
    createInfo.extent.depth  = 1;
    createInfo.mipLevels     = 4;
    createInfo.arrayLayers   = 1;
    createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling        = description.Tiling;
    createInfo.usage         = description.UsageFlags;
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

    if (createView)
    {
        VulkanCreateImageView(context, description.Format, description.AspectFlags, out);
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

void VulkanTransitionImageLayout(
    VulkanContext* context,
    VulkanCommandBuffer commandBuffer,
    VulkanImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image.Handle;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex;
    barrier.dstQueueFamilyIndex = context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        KASSERTM(false, "Transition not supported");
        return;
    }

    vkCmdPipelineBarrier(commandBuffer.Handle, srcStage, dstStage, 0, 0, 0, 0, 0, 1, &barrier);
}

void VulkanCopyBufferToImage(
    VulkanContext* context,
    VulkanCommandBuffer commandBuffer,
    VulkanBuffer srcBuffer,
    VulkanImage* dstImage
)
{
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;

    region.imageExtent.width = dstImage->Width;
    region.imageExtent.height = dstImage->Height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandBuffer.Handle, srcBuffer.Handle, dstImage->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
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