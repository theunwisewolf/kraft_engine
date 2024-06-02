
#include "kraft_vulkan_texture.h"

#include "core/kraft_memory.h"
#include "renderer/vulkan/kraft_vulkan_image.h"
#include "renderer/vulkan/kraft_vulkan_buffer.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"
#include "resources/kraft_resource_types.h"

namespace kraft::renderer
{

void VulkanCreateTexture(VulkanContext* context, uint8* data, Texture* texture)
{
    texture->Generation = KRAFT_INVALID_ID;
    texture->RendererData = Malloc(sizeof(VulkanTexture), MEMORY_TAG_TEXTURE, true);
    
    VulkanTexture* vulkanTexture = (VulkanTexture*)texture->RendererData;

    VkDeviceSize deviceSize = texture->Width * texture->Height * texture->Channels;
    VkFormat     format = texture->Channels == 4 ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8_UNORM;

    // Load data into a staging buffer
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VulkanBuffer stagingBuffer;
    VulkanCreateBuffer(context, (uint64)deviceSize, VK_SHARING_MODE_EXCLUSIVE, usageFlags, memoryProperties, true, &stagingBuffer);
    VulkanLoadDataInBuffer(context, &stagingBuffer, data, (uint64)deviceSize, 0);

    VulkanImageDescription desc;
    desc.Width = texture->Width;
    desc.Height = texture->Height;
    desc.Format = format;
    desc.UsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    desc.Tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.Type = VK_IMAGE_TYPE_2D;
    desc.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    desc.MipLevels = 1; // TODO (amn): Mip-mapping
    desc.Samples = VK_SAMPLE_COUNT_1_BIT; // TODO (amn): What should go here?

    VulkanCreateImage(context, desc, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, &vulkanTexture->Image);

    // Init a single use command buffer
    VulkanCommandBuffer tempCommandBuffer;
    VulkanAllocateAndBeginSingleUseCommandBuffer(context, context->GraphicsCommandPool, &tempCommandBuffer);

    // Default image layout is undefined, so make it transfer dst optimal
    // This will change the image layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VulkanTransitionImageLayout(context, tempCommandBuffer, vulkanTexture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the image pixels from the staging buffer to the image using the temp command buffer
    VulkanCopyBufferToImage(context, tempCommandBuffer, stagingBuffer, &vulkanTexture->Image);

    // Transition the layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    // so the image turns into a suitable format for the shader to read 
    VulkanTransitionImageLayout(context, tempCommandBuffer, vulkanTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // End our temp command buffer
    VulkanEndAndSubmitSingleUseCommandBuffer(context, context->GraphicsCommandPool, &tempCommandBuffer, context->LogicalDevice.GraphicsQueue);

    // Free the staging buffer
    VulkanDestroyBuffer(context, &stagingBuffer);

    // Create the texture sampler
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType           = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter       = VK_FILTER_LINEAR;
    samplerInfo.minFilter       = VK_FILTER_LINEAR;
    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.borderColor     = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.compareEnable   = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias      = 0.0f;
    samplerInfo.minLod          = 0.0f;
    samplerInfo.maxLod          = 0.0f;
    samplerInfo.maxAnisotropy   = 16.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    KRAFT_VK_CHECK(vkCreateSampler(context->LogicalDevice.Handle, &samplerInfo, context->AllocationCallbacks, &vulkanTexture->Sampler));

    texture->Generation++;
}

void VulkanDestroyTexture(VulkanContext* context, Texture* texture)
{
    vkDeviceWaitIdle(context->LogicalDevice.Handle);

    VulkanTexture* internalTexture = (VulkanTexture*)texture->RendererData;
    VulkanDestroyImage(context, &internalTexture->Image);

    vkDestroySampler(context->LogicalDevice.Handle, internalTexture->Sampler, context->AllocationCallbacks);

    Free(internalTexture, sizeof(VulkanTexture), MEMORY_TAG_TEXTURE);
}

}