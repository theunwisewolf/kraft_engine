
#include "kraft_vulkan_texture.h"

#include "core/kraft_memory.h"
#include "renderer/vulkan/kraft_vulkan_image.h"
#include "renderer/vulkan/kraft_vulkan_buffer.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

void VulkanCreateTexture(VulkanContext* context, uint8* data, Texture* texture)
{
    texture->Generation = KRAFT_INVALID_ID;
    texture->RendererData = (VulkanTexture*)Malloc(sizeof(VulkanTexture), MEMORY_TAG_TEXTURE, true);
    
    VulkanTexture* vulkanTexture = (VulkanTexture*)texture->RendererData;

    VkDeviceSize deviceSize = texture->Width * texture->Height * 4;
    VkFormat     format = VK_FORMAT_R8G8B8A8_UNORM;

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

    VulkanCreateImage(context, desc, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, &vulkanTexture->Image);

    // Init a single use command buffer
    VulkanCommandBuffer tempCommandBuffer;
    VulkanAllocateAndBeginSingleUseCommandBuffer(context, context->GraphicsCommandPool, &tempCommandBuffer);

    // Default image layout is undefined, so make it transfer optimal
    VulkanTransitionImageLayout(context, tempCommandBuffer, vulkanTexture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // This will change the image layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VulkanCopyBufferToImage(context, tempCommandBuffer, stagingBuffer, &vulkanTexture->Image);

    // Transition the layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    VulkanTransitionImageLayout(context, tempCommandBuffer, vulkanTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // End our temp command buffer
    VulkanEndAndSubmitSingleUseCommandBuffer(context, context->GraphicsCommandPool, &tempCommandBuffer, context->LogicalDevice.GraphicsQueue);

    // Free the staging buffer
    VulkanDestroyBuffer(context, &stagingBuffer);

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
    samplerInfo.maxAnisotropy   = 16;
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

void VulkanCreateDefaultTexture(VulkanContext *context, uint32 width, uint32 height, uint8 channels, Texture* out)
{
    if (!out)
        out = (Texture*)Malloc(sizeof(Texture));

    out->Width = width;
    out->Height = height;
    out->Channels = channels;

    uint8* pixels = (uint8*)Malloc(width * height * channels, MEMORY_TAG_TEXTURE);
    MemSet(pixels, 255, width * height * channels);

    const int segments = 8, boxSize = width / segments;
    unsigned char white[4] = {255, 255, 255, 255};
    unsigned char black[4] = {0, 0, 0, 255};
    unsigned char* color = NULL;
    int swap = 0;
    int fill = 0;
    for(int i = 0; i < width * height; i++)
    {
        if (i > 0)
        {
            if (i % (width * boxSize) == 0)
                swap = !swap;

            if (i % boxSize == 0)
                fill = !fill;
        }

        if (fill)
        {
            if (swap)
                color = black;
            else
                color = white;
        }
        else
        {
            if (swap)
                color = white;
            else
                color = black;
        }

        for(int j = 0; j < channels; j++)
        {
            pixels[i * channels + j] = color[j];
        }
    }

    // for (int i = 0; i < width; i++)
    // {
    //     for (int j = 0; j < height; j++)
    //     {
    //         uint8 color = (((i & 0x8) == 0) ^ ((j & 0x8)  == 0)) * 255;
    //         int index = i * width + j;
    //         int indexChannel = index * channels;

    //         if (i % 2)
    //         {
    //             if (j % 2)
    //             {
    //                 pixels[indexChannel + 0] = 0;
    //                 pixels[indexChannel + 1] = 0;
    //                 pixels[indexChannel + 2] = 0;
    //             }
    //         }
    //         else if (!(j % 2))
    //         {
    //             pixels[indexChannel + 0] = 0;
    //             pixels[indexChannel + 1] = 0;
    //             pixels[indexChannel + 2] = 0;
    //         }
    //     }
    // }

    VulkanCreateTexture(context, pixels, out);
    Free(pixels);
}

}