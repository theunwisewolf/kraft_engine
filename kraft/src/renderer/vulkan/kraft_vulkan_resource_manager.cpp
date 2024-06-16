#include "kraft_vulkan_resource_manager.h"

#include <renderer/vulkan/kraft_vulkan_helpers.h>
#include <renderer/vulkan/kraft_vulkan_backend.h>
#include <renderer/vulkan/kraft_vulkan_memory.h>
#include <renderer/vulkan/kraft_vulkan_image.h>
#include <renderer/vulkan/kraft_vulkan_command_buffer.h>

namespace kraft::renderer
{

VulkanTempMemoryBlockAllocator::VulkanTempMemoryBlockAllocator(uint64 BlockSize)
{
    this->BlockSize = BlockSize;
}

VulkanTempMemoryBlockAllocator::Block VulkanTempMemoryBlockAllocator::GetNextFreeBlock()
{
    Handle<Buffer> Buf = ResourceManager::Get()->CreateBuffer({
        .Size = this->BlockSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_SRC | BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
    });

    KASSERT(Buf != Handle<Buffer>::Invalid());

    return Block{
        .Ptr = ResourceManager::Get()->GetBufferData(Buf),
        .GPUBuffer = Buf,
    };
}

VulkanResourceManager::VulkanResourceManager() :
    TexturePool(1024),
    BufferPool(1024)
{
    TempGPUAllocator.Allocator = new VulkanTempMemoryBlockAllocator(1024 * 1024 * 128);
}

VulkanResourceManager::~VulkanResourceManager()
{
    delete TempGPUAllocator.Allocator;

    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice Device = VulkanRendererBackend::Device();

    for (int i = 0; i < this->TexturePool.GetSize(); i++)
    {
        VulkanTexture Texture = this->TexturePool.Data[i];
        if (Texture.View)
        {
            vkDestroyImageView(Device, Texture.View, Context->AllocationCallbacks);
        }

        if (Texture.Memory)
        {
            vkFreeMemory(Device, Texture.Memory, Context->AllocationCallbacks);
        }

        if (Texture.Image)
        {
            vkDestroyImage(Device, Texture.Image, Context->AllocationCallbacks);
        }

        vkDestroySampler(Device, Texture.Sampler, Context->AllocationCallbacks);
    }

    for (int i = 0; i < this->BufferPool.GetSize(); i++)
    {
        VulkanBuffer Buffer = this->BufferPool.Data[i];
        vkFreeMemory(Device, Buffer.Memory, Context->AllocationCallbacks);
        vkDestroyBuffer(Device, Buffer.Handle, Context->AllocationCallbacks);
    }
}

Handle<Texture> VulkanResourceManager::CreateTexture(TextureDescription Description)
{
    KASSERT(Description.Dimensions.x > 0 && Description.Dimensions.y > 0 && Description.Dimensions.z > 0 && Description.Dimensions.w > 0);

    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice Device = VulkanRendererBackend::Device();
    VulkanTexture Output = {};

    // Create a VkImage
    if (Description.Tiling == TextureTiling::Linear)
    {
        KASSERT(Description.Type == TextureType::Type2D);
        // TODO: Depth format restrictions check
    }

    VkImageCreateInfo ImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ImageCreateInfo.imageType     = ToVulkanImageType(Description.Type);
    ImageCreateInfo.format        = ToVulkanFormat(Description.Format);
    ImageCreateInfo.extent.width  = Description.Dimensions.x;
    ImageCreateInfo.extent.height = Description.Dimensions.y;
    ImageCreateInfo.extent.depth  = Description.Dimensions.z;
    ImageCreateInfo.mipLevels     = Description.MipLevels;
    ImageCreateInfo.arrayLayers   = 1;
    ImageCreateInfo.samples       = ToVulkanSampleCountFlagBits(Description.SampleCount);
    ImageCreateInfo.tiling        = ToVulkanImageTiling(Description.Tiling);
    ImageCreateInfo.usage         = ToVulkanImageUsageFlags(Description.Usage);
    ImageCreateInfo.sharingMode   = ToVulkanSharingMode(Description.SharingMode);
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // If we have a depth format, ensure it is supported by the physical device
    if (Description.Format >= Format::D16_UNORM && ImageCreateInfo.format != Context->PhysicalDevice.DepthBufferFormat)
    {
        VkImageFormatProperties FormatProperties;
        VkResult Result = vkGetPhysicalDeviceImageFormatProperties(Context->PhysicalDevice.Handle, ImageCreateInfo.format, ImageCreateInfo.imageType, ImageCreateInfo.tiling, ImageCreateInfo.usage, ImageCreateInfo.flags, &FormatProperties);
        if (Result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            KWARN("Supplied image format: %s is not supported by the GPU; Falling back to default depth buffer format.", Format::String(Description.Format));
            ImageCreateInfo.format = Context->PhysicalDevice.DepthBufferFormat;
        }
    }

    // Ensure the image format is supported
    VkResult Result = vkCreateImage(Device, &ImageCreateInfo, Context->AllocationCallbacks, &Output.Image);
    if (Result != VK_SUCCESS)
    {
        return Handle<Texture>::Invalid();
    }

    Context->SetObjectName((uint64)Output.Image, VK_OBJECT_TYPE_IMAGE, Description.DebugName);

    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(Device, Output.Image, &MemoryRequirements);
    if (!VulkanAllocateMemory(Context, MemoryRequirements.size, MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &Output.Memory))
    {
        return Handle<Texture>::Invalid();
    }

    Context->SetObjectName((uint64)Output.Memory, VK_OBJECT_TYPE_DEVICE_MEMORY, Description.DebugName);

    KRAFT_VK_CHECK(vkBindImageMemory(Device, Output.Image, Output.Memory, 0));

    // Now create the image view
    VkImageViewCreateInfo ImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ImageViewCreateInfo.image = Output.Image;
    ImageViewCreateInfo.viewType = Description.Type == TextureType::Type2D ? VK_IMAGE_VIEW_TYPE_2D : (Description.Type == TextureType::Type1D ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_3D);
    ImageViewCreateInfo.format = ImageCreateInfo.format;

    if (
        Description.Format == Format::RGB8_UNORM || 
        Description.Format == Format::RGBA8_UNORM ||
        Description.Format == Format::BGRA8_UNORM ||
        Description.Format == Format::BGR8_UNORM
    )
    {
        ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    else
    {
        ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (Description.Format >= Format::D16_UNORM_S8_UINT)
        {
            ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }

    ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    ImageViewCreateInfo.subresourceRange.layerCount = 1;
    ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    ImageViewCreateInfo.subresourceRange.levelCount = 1;

    KRAFT_VK_CHECK(vkCreateImageView(Device, &ImageViewCreateInfo, Context->AllocationCallbacks, &Output.View));
    Context->SetObjectName((uint64)Output.View, VK_OBJECT_TYPE_IMAGE_VIEW, Description.DebugName);

    // Texture sampler
    if (Description.CreateSampler)
    {
        VkSamplerCreateInfo SamplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        SamplerInfo.magFilter       = ToVulkanFilter(Description.Sampler.MagFilter);
        SamplerInfo.minFilter       = ToVulkanFilter(Description.Sampler.MinFilter);
        SamplerInfo.addressModeU    = ToVulkanSamplerAddressMode(Description.Sampler.WrapModeU);
        SamplerInfo.addressModeV    = ToVulkanSamplerAddressMode(Description.Sampler.WrapModeV);
        SamplerInfo.addressModeW    = ToVulkanSamplerAddressMode(Description.Sampler.WrapModeW);
        SamplerInfo.borderColor     = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        SamplerInfo.compareEnable   = Description.Sampler.Compare > CompareOp::Never ? VK_TRUE : VK_FALSE;
        SamplerInfo.compareOp       = ToVulkanCompareOp(Description.Sampler.Compare);
        SamplerInfo.mipmapMode      = ToVulkanSamplerMipMapMode(Description.Sampler.MipMapMode);
        SamplerInfo.maxAnisotropy   = 16.0f;
        SamplerInfo.anisotropyEnable = Description.Sampler.AnisotropyEnabled;
        SamplerInfo.unnormalizedCoordinates = VK_FALSE;

        KRAFT_VK_CHECK(vkCreateSampler(Device, &SamplerInfo, Context->AllocationCallbacks, &Output.Sampler));
    }

    return TexturePool.Insert({ .Width = Description.Dimensions.x, .Height = Description.Dimensions.y, .Channels = (uint8)Description.Dimensions.w }, Output);
}

void VulkanResourceManager::DestroyTexture(Handle<Texture> Resource)
{
    this->TexturePool.MarkForDelete(Resource);
}

Handle<Buffer> VulkanResourceManager::CreateBuffer(BufferDescription Description)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice Device = VulkanRendererBackend::Device();
    VulkanBuffer Output = {};
    VkMemoryPropertyFlags MemoryProperties = ToVulkanMemoryPropertyFlags(Description.MemoryPropertyFlags);

    VkBufferCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    CreateInfo.size = Description.Size;
    CreateInfo.sharingMode = ToVulkanSharingMode(Description.SharingMode);
    CreateInfo.usage = ToVulkanBufferUsage(Description.UsageFlags);
    
    KRAFT_VK_CHECK(vkCreateBuffer(Device, &CreateInfo, Context->AllocationCallbacks, &Output.Handle));

    VkMemoryRequirements MemoryRequirements;
    vkGetBufferMemoryRequirements(Device, Output.Handle, &MemoryRequirements);

    if (!VulkanAllocateMemory(Context, MemoryRequirements.size, MemoryRequirements.memoryTypeBits, MemoryProperties, &Output.Memory))
    {
        return Handle<Buffer>::Invalid();
    }

    if (Description.BindMemory)
    {
        KRAFT_VK_CHECK(vkBindBufferMemory(Device, Output.Handle, Output.Memory, 0));
    }

    void* Ptr = nullptr;
    if (Description.MapMemory)
    {
        vkMapMemory(VulkanRendererBackend::Device(), Output.Memory, 0, VK_WHOLE_SIZE, 0, &Ptr);
    }

    return BufferPool.Insert({.Size = Description.Size, .Ptr = Ptr}, Output);
}

void VulkanResourceManager::DestroyBuffer(Handle<Buffer> Resource)
{
    this->BufferPool.MarkForDelete(Resource);
}

uint8* VulkanResourceManager::GetBufferData(Handle<Buffer> BufferHandle)
{
    Buffer* Buffer = BufferPool.GetAuxiliaryData(BufferHandle);
    VulkanBuffer* GPUBuffer = BufferPool.Get(BufferHandle);

    // Map the buffer first if it not mapped yet
    if (!Buffer->Ptr)
    {
        KRAFT_VK_CHECK(vkMapMemory(VulkanRendererBackend::Device(), GPUBuffer->Memory, 0, VK_WHOLE_SIZE, 0, &Buffer->Ptr));
    }

    return (uint8*)Buffer->Ptr;
}

TempBuffer VulkanResourceManager::CreateTempBuffer(uint64 Size)
{
    return this->TempGPUAllocator.Allocate(Size, 128);
}

bool VulkanResourceManager::UploadTexture(Handle<Texture> Resource, Handle<Buffer> BufferHandle, uint64 BufferOffset)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    Texture* TextureMetadata = this->TexturePool.GetAuxiliaryData(Resource);
    KASSERT(TextureMetadata);

    VulkanTexture* GPUTexture = this->TexturePool.Get(Resource);
    KASSERT(GPUTexture);

    VulkanBuffer* GPUBuffer = BufferPool.Get(BufferHandle);
    KASSERT(GPUBuffer);

    // Init a single use command buffer
    VulkanCommandBuffer tempCommandBuffer;
    VulkanAllocateAndBeginSingleUseCommandBuffer(Context, Context->GraphicsCommandPool, &tempCommandBuffer);

    // Default image layout is undefined, so make it transfer dst optimal
    // This will change the image layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VulkanTransitionImageLayout(Context, tempCommandBuffer, GPUTexture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the image pixels from the staging buffer to the image using the temp command buffer
    VkBufferImageCopy Region = {};
    Region.bufferOffset = BufferOffset;
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.baseArrayLayer = 0;
    Region.imageSubresource.layerCount = 1;
    Region.imageSubresource.mipLevel = 0;

    Region.imageExtent.width = TextureMetadata->Width;
    Region.imageExtent.height = TextureMetadata->Height;
    Region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(tempCommandBuffer.Handle, GPUBuffer->Handle, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    // Transition the layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    // so the image turns into a suitable format for the shader to read 
    VulkanTransitionImageLayout(Context, tempCommandBuffer, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // End our temp command buffer
    VulkanEndAndSubmitSingleUseCommandBuffer(Context, Context->GraphicsCommandPool, &tempCommandBuffer, Context->LogicalDevice.GraphicsQueue);

    return true;
}

bool VulkanResourceManager::UploadBuffer(UploadBufferDescription Description)
{
    VulkanBuffer *Src = this->BufferPool.Get(Description.SrcBuffer);
    if (!Src)
    {
        return false;
    }

    VulkanBuffer *Dst = this->BufferPool.Get(Description.DstBuffer);
    if (!Dst)
    {
        return false;
    }

    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice Device = VulkanRendererBackend::Device();
    vkDeviceWaitIdle(Device);

    VulkanCommandBuffer TempCommandBuffer;
    VulkanAllocateAndBeginSingleUseCommandBuffer(Context, Context->GraphicsCommandPool, &TempCommandBuffer);

    VkBufferCopy BufferCopyInfo = {};
    BufferCopyInfo.size = Description.SrcSize;
    BufferCopyInfo.srcOffset = Description.SrcOffset;
    BufferCopyInfo.dstOffset = Description.DstOffset;

    vkCmdCopyBuffer(TempCommandBuffer.Handle, Src->Handle, Dst->Handle, 1, &BufferCopyInfo);
    VulkanEndAndSubmitSingleUseCommandBuffer(Context, Context->GraphicsCommandPool, &TempCommandBuffer, Context->LogicalDevice.GraphicsQueue);

    return true;
}

Texture *VulkanResourceManager::GetTextureMetadata(Handle<Texture> Resource)
{
    return TexturePool.GetAuxiliaryData(Resource);
}

void VulkanResourceManager::StartFrame(uint64 FrameNumber)
{
    
}

void VulkanResourceManager::EndFrame(uint64 FrameNumber)
{
    this->TexturePool.Cleanup([](VulkanTexture* Texture){
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice Device = VulkanRendererBackend::Device();

        if (Texture->View)
        {
            vkDestroyImageView(Device, Texture->View, Context->AllocationCallbacks);
            Texture->View = 0;
        }

        if (Texture->Memory)
        {
            vkFreeMemory(Device, Texture->Memory, Context->AllocationCallbacks);
            Texture->Memory = 0;
        }

        if (Texture->Image)
        {
            vkDestroyImage(Device, Texture->Image, Context->AllocationCallbacks);
            Texture->Image = 0;
        }

        vkDestroySampler(Device, Texture->Sampler, Context->AllocationCallbacks);
    });

    this->BufferPool.Cleanup([](VulkanBuffer* GPUResource) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice Device = VulkanRendererBackend::Device();

        vkFreeMemory(Device, GPUResource->Memory, Context->AllocationCallbacks);
        vkDestroyBuffer(Device, GPUResource->Handle, Context->AllocationCallbacks);
    });
}

}