#include "kraft_vulkan_resource_manager.h"

#include <volk/volk.h>

#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <renderer/kraft_resource_pool.inl>
#include <renderer/vulkan/kraft_vulkan_backend.h>
#include <renderer/vulkan/kraft_vulkan_command_buffer.h>
#include <renderer/vulkan/kraft_vulkan_helpers.h>
#include <renderer/vulkan/kraft_vulkan_image.h>
#include <renderer/vulkan/kraft_vulkan_memory.h>

#include <resources/kraft_resource_types.h>

namespace kraft::renderer {

static VkImageAspectFlags GetImageAspectMask(Format::Enum Type)
{
    if (Type == Format::RGB8_UNORM || Type == Format::RGBA8_UNORM || Type == Format::BGRA8_UNORM || Type == Format::BGR8_UNORM || Type == Format::R32_SFLOAT)
    {
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (Type == Format::D16_UNORM || Type == Format::D32_SFLOAT)
    {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (Type >= Format::D16_UNORM_S8_UINT)
    {
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
}

struct VulkanResourceManagerDataT
{
    Pool<VulkanTexture, Texture>             TexturePool = Pool<VulkanTexture, Texture>(1024);
    Pool<VulkanBuffer, Buffer>               BufferPool = Pool<VulkanBuffer, Buffer>(1024);
    Pool<VulkanRenderPass, RenderPass>       RenderPassPool = Pool<VulkanRenderPass, RenderPass>(16);
    Pool<VulkanCommandBuffer, CommandBuffer> CmdBufferPool = Pool<VulkanCommandBuffer, CommandBuffer>(16);
    Pool<VulkanCommandPool, CommandPool>     CmdPoolPool = Pool<VulkanCommandPool, CommandPool>(1);
} VulkanResourceManagerData;

const Pool<VulkanTexture, Texture>& VulkanResourceManager::GetTexturePool() const
{
    return VulkanResourceManagerData.TexturePool;
}

const Pool<VulkanBuffer, Buffer>& VulkanResourceManager::GetBufferPool() const
{
    return VulkanResourceManagerData.BufferPool;
}

const Pool<VulkanRenderPass, RenderPass>& VulkanResourceManager::GetRenderPassPool() const
{
    return VulkanResourceManagerData.RenderPassPool;
}

const Pool<VulkanCommandBuffer, CommandBuffer>& VulkanResourceManager::GetCommandBufferPool() const
{
    return VulkanResourceManagerData.CmdBufferPool;
}

const Pool<VulkanCommandPool, CommandPool>& VulkanResourceManager::GetCommandPoolPool() const
{
    return VulkanResourceManagerData.CmdPoolPool;
}

VulkanTempMemoryBlockAllocator::VulkanTempMemoryBlockAllocator(uint64 BlockSize)
{
    this->BlockSize = BlockSize;
}

GPUBuffer VulkanTempMemoryBlockAllocator::GetNextFreeBlock()
{
    Handle<Buffer> Buf = ResourceManager::Get()->CreateBuffer({
        .DebugName = "VulkanTempMemoryBlockAllocator",
        .Size = this->BlockSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_SRC | BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
        .MapMemory = true,
    });

    KASSERT(Buf != Handle<Buffer>::Invalid());

    return GPUBuffer{
        .GPUBuffer = Buf,
        .Ptr = ResourceManager::Get()->GetBufferData(Buf),
    };
}

VulkanResourceManager::VulkanResourceManager()
{
    TempGPUAllocator.Allocator = new VulkanTempMemoryBlockAllocator(1024 * 1024 * 128);
}

void VulkanResourceManager::Clear()
{
    delete TempGPUAllocator.Allocator;

    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;

    // We don't have to check if the handle to any resource is invalid here in any of the cleanup code
    // because vk___ functions don't do anything if the passed handle is "NULL"

    for (int i = 0; i < VulkanResourceManagerData.RenderPassPool.GetSize(); i++)
    {
        VulkanRenderPass& RenderPass = VulkanResourceManagerData.RenderPassPool.Data[i];
        vkDeviceWaitIdle(Device);

        vkDestroyFramebuffer(Device, RenderPass.Framebuffer.Handle, Context->AllocationCallbacks);
        vkDestroyRenderPass(Device, RenderPass.Handle, Context->AllocationCallbacks);

        RenderPass.Framebuffer.Handle = 0;
        RenderPass.Handle = 0;
    }

    for (int i = 0; i < VulkanResourceManagerData.TexturePool.GetSize(); i++)
    {
        VulkanTexture& Texture = VulkanResourceManagerData.TexturePool.Data[i];
        if (Texture.View)
        {
            vkDestroyImageView(Device, Texture.View, Context->AllocationCallbacks);
            Texture.View = 0;
        }

        if (Texture.Memory)
        {
            vkFreeMemory(Device, Texture.Memory, Context->AllocationCallbacks);
            Texture.Memory = 0;
        }

        if (Texture.Image)
        {
            vkDestroyImage(Device, Texture.Image, Context->AllocationCallbacks);
            Texture.Image = 0;
        }

        vkDestroySampler(Device, Texture.Sampler, Context->AllocationCallbacks);
        Texture.Sampler = 0;
    }

    for (int i = 0; i < VulkanResourceManagerData.BufferPool.GetSize(); i++)
    {
        VulkanBuffer& Buffer = VulkanResourceManagerData.BufferPool.Data[i];
        vkFreeMemory(Device, Buffer.Memory, Context->AllocationCallbacks);
        vkDestroyBuffer(Device, Buffer.Handle, Context->AllocationCallbacks);

        Buffer.Memory = 0;
        Buffer.Handle = 0;
    }

    // We don't have to release command buffers individually, we can just delete the pool they were allocated from
    // for (int i = 0; i < VulkanResourceManagerData.CmdBufferPool.GetSize(); i++)
    // {
    //     CommandBuffer&       CmdBufferMetadata = VulkanResourceManagerData.CmdBufferPool.AuxiliaryData[i];
    //     VulkanCommandBuffer& CmdBuffer = VulkanResourceManagerData.CmdBufferPool.Data[i];

    //     vkFreeCommandBuffers(Device, CmdBuffer.Pool, 1, &CmdBuffer.Resource);
    //     CmdBuffer.Resource = 0;
    //     CmdBuffer.Pool = 0;
    //     CmdBuffer.State = VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    // }

    for (int i = 0; i < VulkanResourceManagerData.CmdPoolPool.GetSize(); i++)
    {
        VulkanCommandPool& CmdPool = VulkanResourceManagerData.CmdPoolPool.Data[i];
        vkDestroyCommandPool(Device, CmdPool.Resource, Context->AllocationCallbacks);
        CmdPool.Resource = 0;
    }
}

Handle<Texture> VulkanResourceManager::CreateTexture(const TextureDescription& Description)
{
    KASSERT(Description.Dimensions.x > 0 && Description.Dimensions.y > 0 && Description.Dimensions.z > 0 && Description.Dimensions.w > 0);

    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;
    VulkanTexture  Output = {};

    // Create a VkImage
    if (Description.Tiling == TextureTiling::Linear)
    {
        KASSERT(Description.Type == TextureType::Type2D);
        // TODO: Depth format restrictions check
    }

    VkImageCreateInfo ImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ImageCreateInfo.imageType = ToVulkanImageType(Description.Type);
    ImageCreateInfo.format = ToVulkanFormat(Description.Format);
    ImageCreateInfo.extent.width = (uint32)Description.Dimensions.x;
    ImageCreateInfo.extent.height = (uint32)Description.Dimensions.y;
    ImageCreateInfo.extent.depth = (uint32)Description.Dimensions.z;
    ImageCreateInfo.mipLevels = Description.MipLevels;
    ImageCreateInfo.arrayLayers = 1;
    ImageCreateInfo.samples = ToVulkanSampleCountFlagBits(Description.SampleCount);
    ImageCreateInfo.tiling = ToVulkanImageTiling(Description.Tiling);
    ImageCreateInfo.usage = ToVulkanImageUsageFlags(Description.Usage);
    ImageCreateInfo.sharingMode = ToVulkanSharingMode(Description.SharingMode);
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // If we have a depth format, ensure it is supported by the physical device
    if (Description.Format >= Format::D16_UNORM && ImageCreateInfo.format != Context->PhysicalDevice.DepthBufferFormat)
    {
        VkImageFormatProperties FormatProperties;
        VkResult                Result = vkGetPhysicalDeviceImageFormatProperties(
            Context->PhysicalDevice.Handle, ImageCreateInfo.format, ImageCreateInfo.imageType, ImageCreateInfo.tiling, ImageCreateInfo.usage, ImageCreateInfo.flags, &FormatProperties
        );
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
    ImageViewCreateInfo.subresourceRange.aspectMask = GetImageAspectMask(Description.Format);
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
        SamplerInfo.magFilter = ToVulkanFilter(Description.Sampler.MagFilter);
        SamplerInfo.minFilter = ToVulkanFilter(Description.Sampler.MinFilter);
        SamplerInfo.addressModeU = ToVulkanSamplerAddressMode(Description.Sampler.WrapModeU);
        SamplerInfo.addressModeV = ToVulkanSamplerAddressMode(Description.Sampler.WrapModeV);
        SamplerInfo.addressModeW = ToVulkanSamplerAddressMode(Description.Sampler.WrapModeW);
        SamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        SamplerInfo.compareEnable = Description.Sampler.Compare > CompareOp::Never ? VK_TRUE : VK_FALSE;
        SamplerInfo.compareOp = ToVulkanCompareOp(Description.Sampler.Compare);
        SamplerInfo.mipmapMode = ToVulkanSamplerMipMapMode(Description.Sampler.MipMapMode);
        SamplerInfo.maxAnisotropy = 16.0f;
        SamplerInfo.anisotropyEnable = Description.Sampler.AnisotropyEnabled;
        SamplerInfo.unnormalizedCoordinates = VK_FALSE;

        KRAFT_VK_CHECK(vkCreateSampler(Device, &SamplerInfo, Context->AllocationCallbacks, &Output.Sampler));
        Context->SetObjectName((uint64)Output.Sampler, VK_OBJECT_TYPE_SAMPLER, Description.DebugName);
    }

    Texture Metadata = {
        .Width = Description.Dimensions.x,
        .Height = Description.Dimensions.y,
        .Channels = (uint8)Description.Dimensions.w,
        .TextureFormat = Description.Format,
        .SampleCount = Description.SampleCount,
    };

    StringCopy(Metadata.DebugName, Description.DebugName);

    return VulkanResourceManagerData.TexturePool.Insert(Metadata, Output);
}

Handle<Buffer> VulkanResourceManager::CreateBuffer(const BufferDescription& Description)
{
    VulkanContext*        Context = VulkanRendererBackend::Context();
    VkDevice              Device = Context->LogicalDevice.Handle;
    VulkanBuffer          Output = {};
    VkMemoryPropertyFlags MemoryProperties = ToVulkanMemoryPropertyFlags(Description.MemoryPropertyFlags);
    uint64                ActualSize = Description.Size; // TODO: Fix

    if (Description.UsageFlags & BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER)
    {
        ActualSize = math::AlignUp(ActualSize, Context->PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment);
    }
    else if (Description.UsageFlags & BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER)
    {
        ActualSize = math::AlignUp(ActualSize, Context->PhysicalDevice.Properties.limits.minStorageBufferOffsetAlignment);
    }

    VkBufferCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    CreateInfo.size = ActualSize;
    CreateInfo.sharingMode = ToVulkanSharingMode(Description.SharingMode);
    CreateInfo.usage = ToVulkanBufferUsage(Description.UsageFlags);

    KRAFT_VK_CHECK(vkCreateBuffer(Device, &CreateInfo, Context->AllocationCallbacks, &Output.Handle));
    Context->SetObjectName((uint64)Output.Handle, VK_OBJECT_TYPE_BUFFER, Description.DebugName);

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
        vkMapMemory(Device, Output.Memory, 0, VK_WHOLE_SIZE, 0, &Ptr);
    }

    return VulkanResourceManagerData.BufferPool.Insert({ .Size = ActualSize, .Ptr = Ptr }, Output);
}

Handle<RenderPass> VulkanResourceManager::CreateRenderPass(const RenderPassDescription& Description)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;

    VkSubpassDescription    Subpasses[8];
    uint32                  SubpassesIndex = 0;
    VkAttachmentReference   DepthAttachmentRef;
    VkAttachmentReference   ColorAttachmentRefs[31] = {};
    uint32                  AttachmentRefsIndex = 0;
    VkAttachmentDescription Attachments[31 + 1] = {};
    VkImageView             FramebufferImageViews[31 + 1] = {};
    uint32                  AttachmentsIndex = 0;

    bool HasDepthTarget = false;
    for (int i = 0; i < Description.Layout.Subpasses.Size(); i++)
    {
        const RenderPassSubpass& Subpass = Description.Layout.Subpasses[i];
        Subpasses[SubpassesIndex] = {};
        Subpasses[SubpassesIndex].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        if (Subpass.DepthTarget)
        {
            HasDepthTarget = true;
            DepthAttachmentRef.attachment = (uint32)Description.ColorTargets.Size(); // Depth attachment will always be at the end
            DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            Subpasses[SubpassesIndex].pDepthStencilAttachment = &DepthAttachmentRef;
        }

        if (Subpass.ColorTargetSlots.Size() > 0)
        {
            Subpasses[SubpassesIndex].colorAttachmentCount = (uint32)Subpass.ColorTargetSlots.Size();
            Subpasses[SubpassesIndex].pColorAttachments = ColorAttachmentRefs + AttachmentRefsIndex;
            for (int j = 0; j < Subpass.ColorTargetSlots.Size(); j++)
            {
                ColorAttachmentRefs[AttachmentRefsIndex].attachment = Subpass.ColorTargetSlots[j];
                ColorAttachmentRefs[AttachmentRefsIndex].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                AttachmentRefsIndex++;
            }
        }

        SubpassesIndex++;
    }

    // Create color attachments
    for (int i = 0; i < Description.ColorTargets.Size(); i++)
    {
        const ColorTarget& Target = Description.ColorTargets[i];
        Texture*           ColorTexture = VulkanResourceManagerData.TexturePool.GetAuxiliaryData(Target.Texture);
        VulkanTexture*     VkColorTexture = VulkanResourceManagerData.TexturePool.Get(Target.Texture);

        Attachments[AttachmentsIndex] = {};
        Attachments[AttachmentsIndex].format = ToVulkanFormat(ColorTexture->TextureFormat);
        Attachments[AttachmentsIndex].loadOp = ToVulkanAttachmentLoadOp(Target.LoadOperation);
        Attachments[AttachmentsIndex].storeOp = ToVulkanAttachmentStoreOp(Target.StoreOperation);
        Attachments[AttachmentsIndex].samples = ToVulkanSampleCountFlagBits(ColorTexture->SampleCount);
        Attachments[AttachmentsIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Attachments[AttachmentsIndex].finalLayout = ToVulkanImageLayout(Target.NextUsage);

        FramebufferImageViews[AttachmentsIndex] = VkColorTexture->View;

        AttachmentsIndex++;
    }

    if (HasDepthTarget)
    {
        Texture*       DepthTexture = VulkanResourceManagerData.TexturePool.GetAuxiliaryData(Description.DepthTarget.Texture);
        VulkanTexture* VkDepthTexture = VulkanResourceManagerData.TexturePool.Get(Description.DepthTarget.Texture);

        Attachments[AttachmentsIndex] = {};
        Attachments[AttachmentsIndex].format = ToVulkanFormat(DepthTexture->TextureFormat);
        Attachments[AttachmentsIndex].loadOp = ToVulkanAttachmentLoadOp(Description.DepthTarget.LoadOperation);
        Attachments[AttachmentsIndex].storeOp = ToVulkanAttachmentStoreOp(Description.DepthTarget.StoreOperation);
        Attachments[AttachmentsIndex].stencilLoadOp = ToVulkanAttachmentLoadOp(Description.DepthTarget.StencilLoadOperation);
        Attachments[AttachmentsIndex].stencilStoreOp = ToVulkanAttachmentStoreOp(Description.DepthTarget.StencilStoreOperation);
        Attachments[AttachmentsIndex].samples = ToVulkanSampleCountFlagBits(DepthTexture->SampleCount);
        Attachments[AttachmentsIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // TODO: If we want to sample the depth-stencil texture, should this layout be
        // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ? Figure this out
        Attachments[AttachmentsIndex].finalLayout = ToVulkanImageLayout(Description.DepthTarget.NextUsage);

        FramebufferImageViews[AttachmentsIndex] = VkDepthTexture->View;

        AttachmentsIndex++;
    }

    VkSubpassDependency dependencies[2];
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_NONE_KHR;
    dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo CreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    CreateInfo.attachmentCount = AttachmentsIndex;
    CreateInfo.pAttachments = Attachments;
    CreateInfo.dependencyCount = 2;
    CreateInfo.pDependencies = dependencies;
    CreateInfo.subpassCount = SubpassesIndex;
    CreateInfo.pSubpasses = Subpasses;

    VulkanRenderPass Out;
    KRAFT_VK_CHECK(vkCreateRenderPass(Device, &CreateInfo, Context->AllocationCallbacks, &Out.Handle));
    Context->SetObjectName((uint64)Out.Handle, VK_OBJECT_TYPE_RENDER_PASS, Description.DebugName);

    Out.Framebuffer.AttachmentCount = AttachmentsIndex;

    VkFramebufferCreateInfo Info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    Info.attachmentCount = AttachmentsIndex;
    Info.pAttachments = FramebufferImageViews;
    Info.width = (uint32)Description.Dimensions.x;
    Info.height = (uint32)Description.Dimensions.y;
    Info.renderPass = Out.Handle;
    Info.layers = 1;

    KRAFT_VK_CHECK(vkCreateFramebuffer(Device, &Info, Context->AllocationCallbacks, &Out.Framebuffer.Handle));
    Context->SetObjectName((uint64)Out.Framebuffer.Handle, VK_OBJECT_TYPE_FRAMEBUFFER, Description.DebugName);

    return VulkanResourceManagerData.RenderPassPool.Insert({ .Dimensions = Description.Dimensions, .DepthTarget = Description.DepthTarget, .ColorTargets = Description.ColorTargets }, Out);
}

Handle<CommandBuffer> VulkanResourceManager::CreateCommandBuffer(const CommandBufferDescription& Description)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;

    // If no command pool is specified, use the default one
    VulkanCommandPool* CmdPool;
    if (Description.CommandPool)
    {
        CmdPool = VulkanResourceManagerData.CmdPoolPool.Get(Description.CommandPool);
    }
    else
    {
        CmdPool = VulkanResourceManagerData.CmdPoolPool.Get(Context->GraphicsCommandPool);
    }

    VkCommandBufferAllocateInfo AllocateInfo = {};
    AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocateInfo.commandPool = CmdPool->Resource;
    AllocateInfo.level = Description.Primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    AllocateInfo.commandBufferCount = 1;

    VulkanCommandBuffer Out;
    KRAFT_VK_CHECK(vkAllocateCommandBuffers(Device, &AllocateInfo, &Out.Resource));
    Out.State = VULKAN_COMMAND_BUFFER_STATE_READY;
    Out.Pool = CmdPool->Resource;

    Context->SetObjectName((uint64)Out.Resource, VK_OBJECT_TYPE_COMMAND_BUFFER, Description.DebugName);

    if (Description.Begin)
    {
        VulkanBeginCommandBuffer(&Out, Description.SingleUse, Description.RenderPassContinue, Description.SimultaneousUse);
    }

    return VulkanResourceManagerData.CmdBufferPool.Insert(
        {
            .Pool = Description.CommandPool,
        },
        Out
    );
}

Handle<CommandPool> VulkanResourceManager::CreateCommandPool(const CommandPoolDescription& Description)
{
    VulkanContext*    Context = VulkanRendererBackend::Context();
    VkDevice          Device = Context->LogicalDevice.Handle;
    VulkanCommandPool Out;

    VkCommandPoolCreateInfo Info = {};
    Info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    Info.queueFamilyIndex = Description.QueueFamilyIndex;
    Info.flags = ToVulkanCommandPoolCreateFlags(Description.Flags);

    KRAFT_VK_CHECK(vkCreateCommandPool(Device, &Info, Context->AllocationCallbacks, &Out.Resource));

    Context->SetObjectName((uint64)Out.Resource, VK_OBJECT_TYPE_COMMAND_POOL, Description.DebugName);

    return VulkanResourceManagerData.CmdPoolPool.Insert({}, Out);
}

void VulkanResourceManager::DestroyTexture(Handle<Texture> Resource)
{
    VulkanResourceManagerData.TexturePool.MarkForDelete(Resource);
}

void VulkanResourceManager::DestroyBuffer(Handle<Buffer> Resource)
{
    VulkanResourceManagerData.BufferPool.MarkForDelete(Resource);
}

void VulkanResourceManager::DestroyRenderPass(Handle<RenderPass> Resource)
{
    VulkanResourceManagerData.RenderPassPool.MarkForDelete(Resource);
}

void VulkanResourceManager::DestroyCommandBuffer(Handle<CommandBuffer> Resource)
{
    // TODO: Should single-use command buffers be destroyed immediately?
    VulkanResourceManagerData.CmdBufferPool.MarkForDelete(Resource);
}

void VulkanResourceManager::DestroyCommandPool(Handle<CommandPool> Resource)
{
    VulkanResourceManagerData.CmdPoolPool.MarkForDelete(Resource);
}

uint8* VulkanResourceManager::GetBufferData(Handle<Buffer> BufferHandle)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;
    Buffer*        Buffer = VulkanResourceManagerData.BufferPool.GetAuxiliaryData(BufferHandle);
    VulkanBuffer*  GPUBuffer = VulkanResourceManagerData.BufferPool.Get(BufferHandle);

    // Map the buffer first if it not mapped yet
    if (!Buffer->Ptr)
    {
        KRAFT_VK_CHECK(vkMapMemory(Device, GPUBuffer->Memory, 0, VK_WHOLE_SIZE, 0, &Buffer->Ptr));
    }

    return (uint8*)Buffer->Ptr;
}

GPUBuffer VulkanResourceManager::CreateTempBuffer(uint64 Size)
{
    return this->TempGPUAllocator.Allocate(Size, 128);
}

bool VulkanResourceManager::UploadTexture(Handle<Texture> Resource, Handle<Buffer> BufferHandle, uint64 BufferOffset)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    Texture*       TextureMetadata = VulkanResourceManagerData.TexturePool.GetAuxiliaryData(Resource);
    KASSERT(TextureMetadata);

    VulkanTexture* GPUTexture = VulkanResourceManagerData.TexturePool.Get(Resource);
    KASSERT(GPUTexture);

    VulkanBuffer* GPUBuffer = VulkanResourceManagerData.BufferPool.Get(BufferHandle);
    KASSERT(GPUBuffer);

    // Allocate a single use command buffer
    Handle<CommandBuffer> TempCmdBuffer = this->CreateCommandBuffer({
        .CommandPool = Context->GraphicsCommandPool,
        .Primary = true,
        .Begin = true,
        .SingleUse = true,
    });

    VulkanCommandBuffer* VkTempCmdBuffer = VulkanResourceManagerData.CmdBufferPool.Get(TempCmdBuffer);

    // Default image layout is undefined, so make it transfer dst optimal
    // This will change the image layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VulkanTransitionImageLayout(Context, VkTempCmdBuffer, GPUTexture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the image pixels from the staging buffer to the image using the temp command buffer
    VkBufferImageCopy Region = {};
    Region.bufferOffset = BufferOffset;
    Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.baseArrayLayer = 0;
    Region.imageSubresource.layerCount = 1;
    Region.imageSubresource.mipLevel = 0;

    Region.imageExtent.width = (uint32)TextureMetadata->Width;
    Region.imageExtent.height = (uint32)TextureMetadata->Height;
    Region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(VkTempCmdBuffer->Resource, GPUBuffer->Handle, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    // Transition the layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    // so the image turns into a suitable format for the shader to read
    VulkanTransitionImageLayout(Context, VkTempCmdBuffer, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // End our temp command buffer
    VulkanEndAndSubmitSingleUseCommandBuffer(Context, VkTempCmdBuffer->Pool, VkTempCmdBuffer, Context->LogicalDevice.GraphicsQueue);
    this->DestroyCommandBuffer(TempCmdBuffer);

    return true;
}

bool VulkanResourceManager::UploadBuffer(const UploadBufferDescription& Description)
{
    VulkanBuffer* Src = VulkanResourceManagerData.BufferPool.Get(Description.SrcBuffer);
    if (!Src)
    {
        return false;
    }

    VulkanBuffer* Dst = VulkanResourceManagerData.BufferPool.Get(Description.DstBuffer);
    if (!Dst)
    {
        return false;
    }

    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;
    vkDeviceWaitIdle(Device);

    // Allocate a single use command buffer
    Handle<CommandBuffer> TempCmdBuffer = this->CreateCommandBuffer({
        .CommandPool = Context->GraphicsCommandPool,
        .Primary = true,
        .Begin = true,
        .SingleUse = true,
    });

    VulkanCommandBuffer* VkTempCmdBuffer = VulkanResourceManagerData.CmdBufferPool.Get(TempCmdBuffer);

    VkBufferCopy BufferCopyInfo = {};
    BufferCopyInfo.size = Description.SrcSize;
    BufferCopyInfo.srcOffset = Description.SrcOffset;
    BufferCopyInfo.dstOffset = Description.DstOffset;

    vkCmdCopyBuffer(VkTempCmdBuffer->Resource, Src->Handle, Dst->Handle, 1, &BufferCopyInfo);
    VulkanEndAndSubmitSingleUseCommandBuffer(Context, VkTempCmdBuffer->Pool, VkTempCmdBuffer, Context->LogicalDevice.GraphicsQueue);
    this->DestroyCommandBuffer(TempCmdBuffer);

    return true;
}

bool VulkanResourceManager::ReadTextureData(const ReadTextureDataDescription& Description)
{
    KASSERT(Description.SrcTexture.IsInvalid() == false);
    KASSERT(Description.OutBuffer);

    Texture*  TextureMetadata = VulkanResourceManagerData.TexturePool.GetAuxiliaryData(Description.SrcTexture);
    uint32    WidthToRead = Description.Width == 0 ? (uint32_t)TextureMetadata->Width : (uint32_t)Description.Width;
    uint32    HeightToRead = Description.Height == 0 ? (uint32_t)TextureMetadata->Height : (uint32_t)Description.Height;
    uint32    StagingBufferSize = WidthToRead * HeightToRead * 4;
    GPUBuffer StagingBuffer = this->CreateTempBuffer(StagingBufferSize);

    if (Description.OutBufferSize < StagingBufferSize)
    {
        return false;
    }

    VulkanContext* Context = VulkanRendererBackend::Context();
    VulkanTexture* GPUTexture = VulkanResourceManagerData.TexturePool.Get(Description.SrcTexture);
    VulkanBuffer*  GPUBuffer = VulkanResourceManagerData.BufferPool.Get(StagingBuffer.GPUBuffer);

    Handle<CommandBuffer> TempCmdBuffer = Description.CmdBuffer;
    VulkanCommandBuffer*  GPUTempCmdBuffer;
    if (Description.CmdBuffer)
    {
        GPUTempCmdBuffer = VulkanResourceManagerData.CmdBufferPool.Get(Description.CmdBuffer);
    }
    else
    {
        // Allocate a single use command buffer
        TempCmdBuffer = this->CreateCommandBuffer({
            .CommandPool = Context->GraphicsCommandPool,
            .Primary = true,
            .Begin = true,
            .SingleUse = true,
        });

        GPUTempCmdBuffer = VulkanResourceManagerData.CmdBufferPool.Get(TempCmdBuffer);
    }

    // We are going to copy the image to a buffer, so convert it to a "Transfer optimal" format
    VulkanTransitionImageLayout(Context, GPUTempCmdBuffer, GPUTexture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    KASSERT(TextureMetadata);
    VkBufferImageCopy Region = { 
        .bufferOffset = StagingBuffer.Offset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = GetImageAspectMask(TextureMetadata->TextureFormat),
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {
            .x = Description.OffsetX,
            .y = Description.OffsetY,
        },
        .imageExtent = {
            .width = WidthToRead,
            .height = HeightToRead,
            .depth = 1,
        },
    };

    vkCmdCopyImageToBuffer(GPUTempCmdBuffer->Resource, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, GPUBuffer->Handle, 1, &Region);

    // Transition the image back to a "shader friendly" layout
    VulkanTransitionImageLayout(Context, GPUTempCmdBuffer, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VulkanEndAndSubmitSingleUseCommandBuffer(Context, GPUTempCmdBuffer->Pool, GPUTempCmdBuffer, Context->LogicalDevice.GraphicsQueue);
    this->DestroyCommandBuffer(TempCmdBuffer);

    MemCpy(Description.OutBuffer, StagingBuffer.Ptr, StagingBufferSize);

    return true;
}

Texture* VulkanResourceManager::GetTextureMetadata(Handle<Texture> Resource)
{
    return VulkanResourceManagerData.TexturePool.GetAuxiliaryData(Resource);
}

void VulkanResourceManager::StartFrame(uint64 FrameNumber)
{}

void VulkanResourceManager::EndFrame(uint64 FrameNumber)
{
    // Render passes will be destroyed first because they may be referencing images
    VulkanResourceManagerData.RenderPassPool.Cleanup([](VulkanRenderPass* RenderPass) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;
        vkDeviceWaitIdle(Device);

        vkDestroyFramebuffer(Device, RenderPass->Framebuffer.Handle, Context->AllocationCallbacks);
        vkDestroyRenderPass(Device, RenderPass->Handle, Context->AllocationCallbacks);

        RenderPass->Framebuffer.Handle = 0;
        RenderPass->Handle = 0;
    });

    VulkanResourceManagerData.TexturePool.Cleanup([](VulkanTexture* Texture) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;

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
        Texture->Sampler = 0;
    });

    VulkanResourceManagerData.BufferPool.Cleanup([](VulkanBuffer* GPUResource) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;

        vkFreeMemory(Device, GPUResource->Memory, Context->AllocationCallbacks);
        vkDestroyBuffer(Device, GPUResource->Handle, Context->AllocationCallbacks);

        GPUResource->Memory = 0;
        GPUResource->Handle = 0;
    });

    VulkanResourceManagerData.CmdBufferPool.Cleanup([](VulkanCommandBuffer* GPUResource) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;
        VkCommandPool  CmdPool = GPUResource->Pool;

        vkFreeCommandBuffers(Device, CmdPool, 1, &GPUResource->Resource);
        GPUResource->Resource = 0;
        GPUResource->State = VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    });

    VulkanResourceManagerData.CmdPoolPool.Cleanup([](VulkanCommandPool* GPUResource) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;

        vkDestroyCommandPool(Device, GPUResource->Resource, Context->AllocationCallbacks);
        GPUResource->Resource = 0;
    });
}

}