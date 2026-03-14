#include <volk/volk.h>

#include "kraft_vulkan_resource_manager.h"

#include <renderer/kraft_resource_pool.inl>

#include <resources/kraft_resource_types.h>

#include "kraft_vulkan_includes.h"

namespace kraft::r {

static i32 FindMemoryIndex(VulkanPhysicalDevice device, u32 type_filter, u32 property_flags)
{
    for (u32 i = 0; i < device.MemoryProperties.memoryTypeCount; ++i)
    {
        VkMemoryType type = device.MemoryProperties.memoryTypes[i];
        if (type_filter & (1 << i) && (type.propertyFlags & property_flags) == property_flags)
        {
            return i;
        }
    }

    return -1;
}

static bool VulkanAllocateMemory(VulkanContext* context, VkDeviceSize size, i32 type_filter, VkMemoryPropertyFlags property_flags, VkMemoryAllocateFlags allocate_flags, VkDeviceMemory* out)
{
    i32 memory_type_index = FindMemoryIndex(context->PhysicalDevice, type_filter, property_flags);
    if (memory_type_index == -1)
    {
        KERROR("[VulkanAllocateMemory]: Failed to find suitable memoryType");
        return false;
    }

    VkMemoryAllocateInfo info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    info.allocationSize = size;
    info.memoryTypeIndex = memory_type_index;

    if (allocate_flags)
    {
        VkMemoryAllocateFlagsInfo flags_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
        flags_info.flags = allocate_flags;
        info.pNext = &flags_info;
    }

    VkResult result = vkAllocateMemory(context->LogicalDevice.Handle, &info, context->AllocationCallbacks, out);
    KRAFT_VK_CHECK(result);

    return true;
}

void VulkanTempMemoryBlockAllocator::Initialize(ArenaAllocator* Arena, uint64 BlockSize)
{
    this->BlockSize = BlockSize;
}

void VulkanTempMemoryBlockAllocator::Destroy()
{}

void VulkanTempMemoryBlockAllocator::Clear()
{
    if (CurrentBlock)
        while (CurrentBlock->Prev)
            CurrentBlock = CurrentBlock->Prev;

    CurrentFreeOffset = 0;
    AllocationCount = 0;
}

BufferView VulkanTempMemoryBlockAllocator::Allocate(ArenaAllocator* Arena, uint64 Size, uint64 Alignment)
{
    if (!CurrentBlock)
    {
        CurrentBlock = ArenaPush(Arena, TempMemoryBlock);
        CurrentBlock->Block = AllocateGPUMemory(Arena);
    }

    KASSERT(Size <= this->BlockSize);
    uint64 Offset = kraft::math::AlignUp(this->CurrentFreeOffset, Alignment);
    this->CurrentFreeOffset = Offset + Size;

    // We cannot fit the incoming request into this block, allocate a new block
    if (this->CurrentFreeOffset > this->BlockSize)
    {
        // Are there any previously allocated blocks ahead of this block?
        if (!CurrentBlock->Next)
        {
            TempMemoryBlock* NewBlock = ArenaPush(Arena, TempMemoryBlock);
            NewBlock->Prev = CurrentBlock;
            CurrentBlock->Next = NewBlock;
            CurrentBlock = NewBlock;
            CurrentBlock->Block = AllocateGPUMemory(Arena);
        }

        Offset = 0;
        this->CurrentFreeOffset = Size;
    }

    KASSERT(this->CurrentBlock->Block.GPUBuffer);

    this->AllocationCount++;
    return {
        .GPUBuffer = CurrentBlock->Block.GPUBuffer,
        .Ptr = CurrentBlock->Block.Ptr + Offset,
        .Offset = Offset,
    };
}

BufferView VulkanTempMemoryBlockAllocator::AllocateGPUMemory(ArenaAllocator* Arena)
{
    Handle<Buffer> Buf = ResourceManager->CreateBuffer({
        .DebugName = "VkTempMemoryBlockAllocator",
        .Size = BlockSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_SRC | BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
        .MapMemory = true,
    });

    KASSERT(Buf != Handle<Buffer>::Invalid());

    return BufferView{
        .GPUBuffer = Buf,
        .Ptr = ResourceManager->GetBufferData(Buf),
    };
}

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

    return VK_IMAGE_ASPECT_COLOR_BIT;
}

static VulkanResourceManagerState* state = nullptr;

static void Clear()
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;

    // We don't have to check if the handle to any resource is invalid here in any of the cleanup code
    // because vk___ functions don't do anything if the passed handle is "NULL"

    for (int i = 0; i < state->RenderPassPool.GetSize(); i++)
    {
        VulkanRenderPass& RenderPass = state->RenderPassPool.Data[i];
        vkDeviceWaitIdle(Device);

        vkDestroyFramebuffer(Device, RenderPass.Framebuffer.Handle, Context->AllocationCallbacks);
        vkDestroyRenderPass(Device, RenderPass.Handle, Context->AllocationCallbacks);

        RenderPass.Framebuffer.Handle = 0;
        RenderPass.Handle = 0;
    }

    for (int i = 0; i < state->TexturePool.GetSize(); i++)
    {
        VulkanTexture& Texture = state->TexturePool.Data[i];
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
    }

    for (int i = 0; i < state->TextureSamplerPool.GetSize(); i++)
    {
        VulkanTextureSampler& TextureSampler = state->TextureSamplerPool.Data[i];

        vkDestroySampler(Device, TextureSampler.Sampler, Context->AllocationCallbacks);
        TextureSampler.Sampler = 0;
    }

    for (int i = 0; i < state->BufferPool.GetSize(); i++)
    {
        VulkanBuffer& Buffer = state->BufferPool.Data[i];
        vkFreeMemory(Device, Buffer.Memory, Context->AllocationCallbacks);
        vkDestroyBuffer(Device, Buffer.Handle, Context->AllocationCallbacks);

        Buffer.Memory = 0;
        Buffer.Handle = 0;
    }

    // We don't have to release command buffers individually, we can just delete the pool they were allocated from
    // for (int i = 0; i < state->CmdBufferPool.GetSize(); i++)
    // {
    //     CommandBuffer&       CmdBufferMetadata = state->CmdBufferPool.AuxiliaryData[i];
    //     VulkanCommandBuffer& CmdBuffer = state->CmdBufferPool.Data[i];

    //     vkFreeCommandBuffers(Device, CmdBuffer.Pool, 1, &CmdBuffer.Resource);
    //     CmdBuffer.Resource = 0;
    //     CmdBuffer.Pool = 0;
    //     CmdBuffer.State = VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    // }

    for (int i = 0; i < state->CmdPoolPool.GetSize(); i++)
    {
        VulkanCommandPool& CmdPool = state->CmdPoolPool.Data[i];
        vkDestroyCommandPool(Device, CmdPool.Resource, Context->AllocationCallbacks);
        CmdPool.Resource = 0;
    }
}

static Handle<Texture> CreateTexture(const TextureDescription& description)
{
    KASSERT(description.Dimensions.x > 0 && description.Dimensions.y > 0 && description.Dimensions.z > 0 && description.Dimensions.w > 0);

    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice       device = context->LogicalDevice.Handle;
    VulkanTexture  output = {};

    // Create a VkImage
    if (description.Tiling == TextureTiling::Linear)
    {
        KASSERT(description.Type == TextureType::Type2D);
        // TODO: Depth format restrictions check
    }

    VkImageCreateInfo create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    create_info.imageType = ToVulkanImageType(description.Type);
    create_info.format = ToVulkanFormat(description.Format);
    create_info.extent.width = (u32)description.Dimensions.x;
    create_info.extent.height = (u32)description.Dimensions.y;
    create_info.extent.depth = (u32)description.Dimensions.z;
    create_info.mipLevels = description.MipLevels;
    create_info.arrayLayers = 1;
    create_info.samples = ToVulkanSampleCountFlagBits(description.SampleCount);
    create_info.tiling = ToVulkanImageTiling(description.Tiling);
    create_info.usage = ToVulkanImageUsageFlags(description.Usage);
    create_info.sharingMode = ToVulkanSharingMode(description.SharingMode);
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // If we have a depth format, ensure it is supported by the physical device
    if (description.Format >= Format::D16_UNORM && create_info.format != context->PhysicalDevice.DepthBufferFormat)
    {
        VkImageFormatProperties format_properties;
        VkResult                result = vkGetPhysicalDeviceImageFormatProperties(
            context->PhysicalDevice.Handle, create_info.format, create_info.imageType, create_info.tiling, create_info.usage, create_info.flags, &format_properties
        );
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            KWARN("Supplied image format: %s is not supported by the GPU; Falling back to default depth buffer format.", Format::String(description.Format));
            create_info.format = context->PhysicalDevice.DepthBufferFormat;
        }
    }

    // Ensure the image format is supported
    VkResult Result = vkCreateImage(device, &create_info, context->AllocationCallbacks, &output.Image);
    if (Result != VK_SUCCESS)
    {
        return Handle<Texture>::Invalid();
    }

    context->SetObjectName((uint64)output.Image, VK_OBJECT_TYPE_IMAGE, description.DebugName);

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(device, output.Image, &memory_requirements);
    if (!VulkanAllocateMemory(context, memory_requirements.size, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, &output.Memory))
    {
        return Handle<Texture>::Invalid();
    }

    context->SetObjectName((uint64)output.Memory, VK_OBJECT_TYPE_DEVICE_MEMORY, description.DebugName);

    KRAFT_VK_CHECK(vkBindImageMemory(device, output.Image, output.Memory, 0));

    // Now create the image view
    VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view_create_info.image = output.Image;
    view_create_info.viewType = description.Type == TextureType::Type2D ? VK_IMAGE_VIEW_TYPE_2D : (description.Type == TextureType::Type1D ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_3D);
    view_create_info.format = create_info.format;
    view_create_info.subresourceRange.aspectMask = GetImageAspectMask(description.Format);
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    // ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    // ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_R;
    // ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_R;
    // ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_ONE;

    KRAFT_VK_CHECK(vkCreateImageView(device, &view_create_info, context->AllocationCallbacks, &output.View));
    context->SetObjectName((u64)output.View, VK_OBJECT_TYPE_IMAGE_VIEW, description.DebugName);

    Texture Metadata = {
        .Width = description.Dimensions.x,
        .Height = description.Dimensions.y,
        .Channels = (u8)description.Dimensions.w,
        .TextureFormat = description.Format,
        .SampleCount = description.SampleCount,
    };

    StringCopy(Metadata.DebugName, description.DebugName);

    return state->TexturePool.Insert(Metadata, output);
}

static Handle<TextureSampler> CreateTextureSampler(const TextureSamplerDescription& Description)
{
    VulkanContext*       Context = VulkanRendererBackend::Context();
    VkDevice             Device = Context->LogicalDevice.Handle;
    VulkanTextureSampler Output = {};

    VkSamplerCreateInfo SamplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    SamplerInfo.magFilter = ToVulkanFilter(Description.MagFilter);
    SamplerInfo.minFilter = ToVulkanFilter(Description.MinFilter);
    SamplerInfo.addressModeU = ToVulkanSamplerAddressMode(Description.WrapModeU);
    SamplerInfo.addressModeV = ToVulkanSamplerAddressMode(Description.WrapModeV);
    SamplerInfo.addressModeW = ToVulkanSamplerAddressMode(Description.WrapModeW);
    SamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    SamplerInfo.compareEnable = Description.Compare > CompareOp::Never ? VK_TRUE : VK_FALSE;
    SamplerInfo.compareOp = ToVulkanCompareOp(Description.Compare);
    SamplerInfo.mipmapMode = ToVulkanSamplerMipMapMode(Description.MipMapMode);
    SamplerInfo.maxAnisotropy = Description.AnisotropyEnabled ? 16.0f : 1.0f;
    SamplerInfo.anisotropyEnable = Description.AnisotropyEnabled;
    SamplerInfo.unnormalizedCoordinates = VK_FALSE;

    KRAFT_VK_CHECK(vkCreateSampler(Device, &SamplerInfo, Context->AllocationCallbacks, &Output.Sampler));
    Context->SetObjectName((uint64)Output.Sampler, VK_OBJECT_TYPE_SAMPLER, Description.DebugName);

    TextureSampler Metadata{};

    return state->TextureSamplerPool.Insert(Metadata, Output);
}

static Handle<Buffer> CreateBuffer(const BufferDescription& Description)
{
    VulkanContext*        context = VulkanRendererBackend::Context();
    VkDevice              device = context->LogicalDevice.Handle;
    VulkanBuffer          output = {};
    VkMemoryPropertyFlags memory_properties = ToVulkanMemoryPropertyFlags(Description.MemoryPropertyFlags);
    VkMemoryAllocateFlags allocate_flags = ToVulkanMemoryAllocateFlags(Description.MemoryAllocationFlags);
    u64                   actual_size = Description.Size; // TODO: Fix

    if (Description.UsageFlags & BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER)
    {
        actual_size = math::AlignUp(actual_size, context->PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment);
    }
    else if (Description.UsageFlags & BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER)
    {
        actual_size = math::AlignUp(actual_size, context->PhysicalDevice.Properties.limits.minStorageBufferOffsetAlignment);
    }

    VkBufferCreateInfo create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    create_info.size = actual_size;
    create_info.sharingMode = ToVulkanSharingMode(Description.SharingMode);
    create_info.usage = ToVulkanBufferUsage(Description.UsageFlags);

    KRAFT_VK_CHECK(vkCreateBuffer(device, &create_info, context->AllocationCallbacks, &output.Handle));
    context->SetObjectName((u64)output.Handle, VK_OBJECT_TYPE_BUFFER, Description.DebugName);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, output.Handle, &memory_requirements);

    if (!VulkanAllocateMemory(context, memory_requirements.size, memory_requirements.memoryTypeBits, memory_properties, allocate_flags, &output.Memory))
    {
        return Handle<Buffer>::Invalid();
    }

    if (Description.BindMemory)
    {
        KRAFT_VK_CHECK(vkBindBufferMemory(device, output.Handle, output.Memory, 0));
    }

    // Get the device address if this buffer is to be used as a raw pointer in shaders
    if (Description.MemoryPropertyFlags & BUFFER_USAGE_FLAGS_SHADER_DEVICE_ADDRESS)
    {
        VkBufferDeviceAddressInfo addr_info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addr_info.buffer = output.Handle;
        output.device_address = vkGetBufferDeviceAddress(device, &addr_info);
    }

    void* ptr = nullptr;
    if (Description.MapMemory)
    {
        vkMapMemory(device, output.Memory, 0, VK_WHOLE_SIZE, 0, &ptr);
    }

    return state->BufferPool.Insert({ .Size = actual_size, .Ptr = ptr }, output);
}

static Handle<RenderPass> CreateRenderPass(const RenderPassDescription& description)
{
#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    return Handle<RenderPass>::Invalid();
#endif

    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice       device = context->LogicalDevice.Handle;

    VkSubpassDescription    subpasses[8];
    u32                     subpasses_index = 0;
    VkAttachmentReference   depth_attachment_ref;
    VkAttachmentReference   color_attachment_refs[31] = {};
    u32                     attachment_refs_index = 0;
    VkAttachmentDescription attachments[31 + 1] = {};
    VkImageView             framebuffer_image_views[31 + 1] = {};
    u32                     attachments_index = 0;

    bool has_depth_target = false;
    for (i32 i = 0; i < description.Layout.Subpasses.Size(); i++)
    {
        const RenderPassSubpass& subpass = description.Layout.Subpasses[i];
        subpasses[subpasses_index] = {};
        subpasses[subpasses_index].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        if (subpass.DepthTarget)
        {
            has_depth_target = true;
            depth_attachment_ref.attachment = (u32)description.ColorTargets.Size(); // Depth attachment will always be at the end
            depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            subpasses[subpasses_index].pDepthStencilAttachment = &depth_attachment_ref;
        }

        if (subpass.ColorTargetSlots.Size() > 0)
        {
            subpasses[subpasses_index].colorAttachmentCount = (u32)subpass.ColorTargetSlots.Size();
            subpasses[subpasses_index].pColorAttachments = color_attachment_refs + attachment_refs_index;
            for (int j = 0; j < subpass.ColorTargetSlots.Size(); j++)
            {
                color_attachment_refs[attachment_refs_index].attachment = subpass.ColorTargetSlots[j];
                color_attachment_refs[attachment_refs_index].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                attachment_refs_index++;
            }
        }

        subpasses_index++;
    }

    // Create color attachments
    for (i32 i = 0; i < description.ColorTargets.Size(); i++)
    {
        const ColorTarget& target = description.ColorTargets[i];
        Texture*           texture = state->TexturePool.GetAuxiliaryData(target.Texture);
        VulkanTexture*     vk_texture = state->TexturePool.Get(target.Texture);

        attachments[attachments_index] = {};
        attachments[attachments_index].format = ToVulkanFormat(texture->TextureFormat);
        attachments[attachments_index].loadOp = ToVulkanAttachmentLoadOp(target.LoadOperation);
        attachments[attachments_index].storeOp = ToVulkanAttachmentStoreOp(target.StoreOperation);
        attachments[attachments_index].samples = ToVulkanSampleCountFlagBits(texture->SampleCount);
        attachments[attachments_index].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[attachments_index].finalLayout = ToVulkanImageLayout(target.NextUsage);

        framebuffer_image_views[attachments_index] = vk_texture->View;

        attachments_index++;
    }

    if (has_depth_target)
    {
        Texture*       depth_texture = state->TexturePool.GetAuxiliaryData(description.DepthTarget.Texture);
        VulkanTexture* vk_depth_texture = state->TexturePool.Get(description.DepthTarget.Texture);

        attachments[attachments_index] = {};
        attachments[attachments_index].format = ToVulkanFormat(depth_texture->TextureFormat);
        attachments[attachments_index].loadOp = ToVulkanAttachmentLoadOp(description.DepthTarget.LoadOperation);
        attachments[attachments_index].storeOp = ToVulkanAttachmentStoreOp(description.DepthTarget.StoreOperation);
        attachments[attachments_index].stencilLoadOp = ToVulkanAttachmentLoadOp(description.DepthTarget.StencilLoadOperation);
        attachments[attachments_index].stencilStoreOp = ToVulkanAttachmentStoreOp(description.DepthTarget.StencilStoreOperation);
        attachments[attachments_index].samples = ToVulkanSampleCountFlagBits(depth_texture->SampleCount);
        attachments[attachments_index].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // TODO: If we want to sample the depth-stencil texture, should this layout be
        // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ? Figure this out
        attachments[attachments_index].finalLayout = ToVulkanImageLayout(description.DepthTarget.NextUsage);

        framebuffer_image_views[attachments_index] = vk_depth_texture->View;

        attachments_index++;
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

    VkRenderPassCreateInfo create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    create_info.attachmentCount = attachments_index;
    create_info.pAttachments = attachments;
    create_info.dependencyCount = 2;
    create_info.pDependencies = dependencies;
    create_info.subpassCount = subpasses_index;
    create_info.pSubpasses = subpasses;

    VulkanRenderPass out_render_pass;
    KRAFT_VK_CHECK(vkCreateRenderPass(device, &create_info, context->AllocationCallbacks, &out_render_pass.Handle));
    context->SetObjectName((u64)out_render_pass.Handle, VK_OBJECT_TYPE_RENDER_PASS, description.DebugName);

    out_render_pass.Framebuffer.AttachmentCount = attachments_index;

    VkFramebufferCreateInfo info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    info.attachmentCount = attachments_index;
    info.pAttachments = framebuffer_image_views;
    info.width = (u32)description.Dimensions.x;
    info.height = (u32)description.Dimensions.y;
    info.renderPass = out_render_pass.Handle;
    info.layers = 1;

    KRAFT_VK_CHECK(vkCreateFramebuffer(device, &info, context->AllocationCallbacks, &out_render_pass.Framebuffer.Handle));
    context->SetObjectName((u64)out_render_pass.Framebuffer.Handle, VK_OBJECT_TYPE_FRAMEBUFFER, description.DebugName);

    return state->RenderPassPool.Insert(
        {
            .Dimensions = description.Dimensions,
            .DepthTarget = description.DepthTarget,
            .ColorTargets = description.ColorTargets,
        },
        out_render_pass
    );
}

static Handle<CommandBuffer> CreateCommandBuffer(const CommandBufferDescription& Description)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;

    // If no command pool is specified, use the default one
    VulkanCommandPool* CmdPool;
    if (Description.CommandPool)
    {
        CmdPool = state->CmdPoolPool.Get(Description.CommandPool);
    }
    else
    {
        CmdPool = state->CmdPoolPool.Get(Context->GraphicsCommandPool);
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

    return state->CmdBufferPool.Insert(
        {
            .Pool = Description.CommandPool,
        },
        Out
    );
}

static Handle<CommandPool> CreateCommandPool(const CommandPoolDescription& Description)
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

    return state->CmdPoolPool.Insert({}, Out);
}

static void DestroyTexture(Handle<Texture> Resource)
{
    state->TexturePool.MarkForDelete(Resource);
}

static void DestroyBuffer(Handle<Buffer> Resource)
{
    state->BufferPool.MarkForDelete(Resource);
}

static void DestroyRenderPass(Handle<RenderPass> Resource)
{
    state->RenderPassPool.MarkForDelete(Resource);
}

static void DestroyCommandBuffer(Handle<CommandBuffer> Resource)
{
    // TODO: Should single-use command buffers be destroyed immediately?
    state->CmdBufferPool.MarkForDelete(Resource);
}

static void DestroyCommandPool(Handle<CommandPool> Resource)
{
    state->CmdPoolPool.MarkForDelete(Resource);
}

static uint8* GetBufferData(Handle<Buffer> BufferHandle)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;
    Buffer*        Buffer = state->BufferPool.GetAuxiliaryData(BufferHandle);
    VulkanBuffer*  GPUBuffer = state->BufferPool.Get(BufferHandle);

    // Map the buffer first if it not mapped yet
    if (!Buffer->Ptr)
    {
        KRAFT_VK_CHECK(vkMapMemory(Device, GPUBuffer->Memory, 0, VK_WHOLE_SIZE, 0, &Buffer->Ptr));
    }

    return (uint8*)Buffer->Ptr;
}

static BufferView CreateTempBuffer(uint64 Size)
{
    return state->TempGPUAllocator->Allocate(state->Arena, Size, 128);
}

static bool UploadTexture(Handle<Texture> Resource, Handle<Buffer> BufferHandle, uint64 BufferOffset)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    Texture*       TextureMetadata = state->TexturePool.GetAuxiliaryData(Resource);
    KASSERT(TextureMetadata);

    VulkanTexture* GPUTexture = state->TexturePool.Get(Resource);
    KASSERT(GPUTexture);

    VulkanBuffer* GPUBuffer = state->BufferPool.Get(BufferHandle);
    KASSERT(GPUBuffer);

    // Allocate a single use command buffer
    Handle<CommandBuffer> TempCmdBuffer = CreateCommandBuffer({
        .CommandPool = Context->GraphicsCommandPool,
        .Primary = true,
        .Begin = true,
        .SingleUse = true,
    });

    VulkanCommandBuffer* VkTempCmdBuffer = state->CmdBufferPool.Get(TempCmdBuffer);

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

    Region.imageExtent.width = (u32)TextureMetadata->Width;
    Region.imageExtent.height = (u32)TextureMetadata->Height;
    Region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(VkTempCmdBuffer->Resource, GPUBuffer->Handle, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

    // Transition the layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    // so the image turns into a suitable format for the shader to read
    VulkanTransitionImageLayout(Context, VkTempCmdBuffer, GPUTexture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // End our temp command buffer
    VulkanEndAndSubmitSingleUseCommandBuffer(Context, VkTempCmdBuffer->Pool, VkTempCmdBuffer, Context->LogicalDevice.GraphicsQueue);
    DestroyCommandBuffer(TempCmdBuffer);

    return true;
}

static bool UploadBuffer(const UploadBufferDescription& Description)
{
    VulkanBuffer* Src = state->BufferPool.Get(Description.SrcBuffer);
    if (!Src)
    {
        return false;
    }

    VulkanBuffer* Dst = state->BufferPool.Get(Description.DstBuffer);
    if (!Dst)
    {
        return false;
    }

    VulkanContext* Context = VulkanRendererBackend::Context();
    VkDevice       Device = Context->LogicalDevice.Handle;
    vkDeviceWaitIdle(Device);

    // Allocate a single use command buffer
    Handle<CommandBuffer> TempCmdBuffer = CreateCommandBuffer({
        .CommandPool = Context->GraphicsCommandPool,
        .Primary = true,
        .Begin = true,
        .SingleUse = true,
    });

    VulkanCommandBuffer* VkTempCmdBuffer = state->CmdBufferPool.Get(TempCmdBuffer);

    VkBufferCopy BufferCopyInfo = {};
    BufferCopyInfo.size = Description.SrcSize;
    BufferCopyInfo.srcOffset = Description.SrcOffset;
    BufferCopyInfo.dstOffset = Description.DstOffset;

    vkCmdCopyBuffer(VkTempCmdBuffer->Resource, Src->Handle, Dst->Handle, 1, &BufferCopyInfo);
    VulkanEndAndSubmitSingleUseCommandBuffer(Context, VkTempCmdBuffer->Pool, VkTempCmdBuffer, Context->LogicalDevice.GraphicsQueue);
    DestroyCommandBuffer(TempCmdBuffer);

    return true;
}

static bool ReadTextureData(const ReadTextureDataDescription& Description)
{
    KASSERT(Description.SrcTexture.IsInvalid() == false);
    KASSERT(Description.OutBuffer);

    Texture*   TextureMetadata = state->TexturePool.GetAuxiliaryData(Description.SrcTexture);
    u32        WidthToRead = Description.Width == 0 ? (u32)TextureMetadata->Width : (u32)Description.Width;
    u32        HeightToRead = Description.Height == 0 ? (u32)TextureMetadata->Height : (u32)Description.Height;
    u32        StagingBufferSize = WidthToRead * HeightToRead * 4;
    BufferView StagingBuffer = CreateTempBuffer(StagingBufferSize);

    if (Description.OutBufferSize < StagingBufferSize)
    {
        return false;
    }

    VulkanContext* Context = VulkanRendererBackend::Context();
    VulkanTexture* GPUTexture = state->TexturePool.Get(Description.SrcTexture);
    VulkanBuffer*  GPUBuffer = state->BufferPool.Get(StagingBuffer.GPUBuffer);

    Handle<CommandBuffer> TempCmdBuffer = Description.CmdBuffer;
    VulkanCommandBuffer*  GPUTempCmdBuffer;
    if (Description.CmdBuffer)
    {
        GPUTempCmdBuffer = state->CmdBufferPool.Get(Description.CmdBuffer);
    }
    else
    {
        // Allocate a single use command buffer
        TempCmdBuffer = CreateCommandBuffer({
            .CommandPool = Context->GraphicsCommandPool,
            .Primary = true,
            .Begin = true,
            .SingleUse = true,
        });

        GPUTempCmdBuffer = state->CmdBufferPool.Get(TempCmdBuffer);
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
    DestroyCommandBuffer(TempCmdBuffer);

    MemCpy(Description.OutBuffer, StagingBuffer.Ptr, StagingBufferSize);

    return true;
}

static Texture* GetTextureMetadata(Handle<Texture> Resource)
{
    return state->TexturePool.GetAuxiliaryData(Resource);
}

static RenderPass* GetRenderPassMetadata(Handle<RenderPass> Resource)
{
    return state->RenderPassPool.GetAuxiliaryData(Resource);
}

static void StartFrame(uint64 FrameNumber)
{}

static void EndFrame(uint64 FrameNumber)
{
    // Render passes will be destroyed first because they may be referencing images
    state->RenderPassPool.Cleanup([](VulkanRenderPass* RenderPass) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;
        vkDeviceWaitIdle(Device);

        vkDestroyFramebuffer(Device, RenderPass->Framebuffer.Handle, Context->AllocationCallbacks);
        vkDestroyRenderPass(Device, RenderPass->Handle, Context->AllocationCallbacks);

        RenderPass->Framebuffer.Handle = 0;
        RenderPass->Handle = 0;
    });

    state->TexturePool.Cleanup([](VulkanTexture* Texture) {
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
    });

    state->TextureSamplerPool.Cleanup([](VulkanTextureSampler* TextureSampler) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;
        vkDestroySampler(Device, TextureSampler->Sampler, Context->AllocationCallbacks);
        TextureSampler->Sampler = 0;
    });

    state->BufferPool.Cleanup([](VulkanBuffer* GPUResource) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;

        vkFreeMemory(Device, GPUResource->Memory, Context->AllocationCallbacks);
        vkDestroyBuffer(Device, GPUResource->Handle, Context->AllocationCallbacks);

        GPUResource->Memory = 0;
        GPUResource->Handle = 0;
    });

    state->CmdBufferPool.Cleanup([](VulkanCommandBuffer* GPUResource) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;
        VkCommandPool  CmdPool = GPUResource->Pool;

        vkFreeCommandBuffers(Device, CmdPool, 1, &GPUResource->Resource);
        GPUResource->Resource = 0;
        GPUResource->State = VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    });

    state->CmdPoolPool.Cleanup([](VulkanCommandPool* GPUResource) {
        VulkanContext* Context = VulkanRendererBackend::Context();
        VkDevice       Device = Context->LogicalDevice.Handle;

        vkDestroyCommandPool(Device, GPUResource->Resource, Context->AllocationCallbacks);
        GPUResource->Resource = 0;
    });

    state->TempGPUAllocator->Clear();
}

VulkanTexture* VulkanResourceManagerApi::GetTexture(Handle<Texture> Resource)
{
    return state->TexturePool.Get(Resource);
}

VulkanTextureSampler* VulkanResourceManagerApi::GetTextureSampler(Handle<TextureSampler> Resource)
{
    return state->TextureSamplerPool.Get(Resource);
}

VulkanBuffer* VulkanResourceManagerApi::GetBuffer(Handle<Buffer> Resource)
{
    return state->BufferPool.Get(Resource);
}

VulkanRenderPass* VulkanResourceManagerApi::GetRenderPass(Handle<RenderPass> Resource)
{
    return state->RenderPassPool.Get(Resource);
}

VulkanCommandBuffer* VulkanResourceManagerApi::GetCommandBuffer(Handle<CommandBuffer> Resource)
{
    return state->CmdBufferPool.Get(Resource);
}

VulkanCommandPool* VulkanResourceManagerApi::GetCommandPool(Handle<CommandPool> Resource)
{
    return state->CmdPoolPool.Get(Resource);
}

struct ResourceManager* CreateVulkanResourceManager(ArenaAllocator* Arena)
{
    state = ArenaPush(Arena, VulkanResourceManagerState);
    state->Arena = Arena;
    state->TexturePool.Grow(KRAFT_RENDERER__MAX_GLOBAL_TEXTURES);
    state->TextureSamplerPool.Grow(4);
    state->BufferPool.Grow(1024);
    state->RenderPassPool.Grow(16);
    state->CmdBufferPool.Grow(16);
    state->CmdPoolPool.Grow(1);
    state->TempGPUAllocator = ArenaPush(Arena, VulkanTempMemoryBlockAllocator);
    state->TempGPUAllocator->Initialize(Arena, KRAFT_SIZE_MB(128));

    struct ResourceManager* Api = ArenaPush(Arena, struct ResourceManager);
    Api->Clear = Clear;
    Api->CreateTexture = CreateTexture;
    Api->CreateTextureSampler = CreateTextureSampler;
    Api->CreateBuffer = CreateBuffer;
    Api->CreateRenderPass = CreateRenderPass;
    Api->CreateCommandBuffer = CreateCommandBuffer;
    Api->CreateCommandPool = CreateCommandPool;
    Api->DestroyTexture = DestroyTexture;
    Api->DestroyBuffer = DestroyBuffer;
    Api->DestroyRenderPass = DestroyRenderPass;
    Api->DestroyCommandBuffer = DestroyCommandBuffer;
    Api->DestroyCommandPool = DestroyCommandPool;
    Api->GetBufferData = GetBufferData;
    Api->CreateTempBuffer = CreateTempBuffer;
    Api->UploadTexture = UploadTexture;
    Api->UploadBuffer = UploadBuffer;
    Api->ReadTextureData = ReadTextureData;
    Api->GetTextureMetadata = GetTextureMetadata;
    Api->GetRenderPassMetadata = GetRenderPassMetadata;
    Api->StartFrame = StartFrame;
    Api->EndFrame = EndFrame;

    return Api;
}

void DestroyVulkanResourceManager(struct ResourceManager* ResourceManager)
{
    state->CmdPoolPool.Destroy();
    state->CmdBufferPool.Destroy();
    state->RenderPassPool.Destroy();
    state->BufferPool.Destroy();
    state->TexturePool.Destroy();
    state->TextureSamplerPool.Destroy();
}
} // namespace kraft::r
