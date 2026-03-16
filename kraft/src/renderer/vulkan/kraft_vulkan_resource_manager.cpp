#include <volk/volk.h>

#include "kraft_includes.h"

namespace kraft::r {

static i32 FindMemoryIndex(VulkanPhysicalDevice device, u32 type_filter, u32 property_flags) {
    for (u32 i = 0; i < device.MemoryProperties.memoryTypeCount; ++i) {
        VkMemoryType type = device.MemoryProperties.memoryTypes[i];
        if (type_filter & (1 << i) && (type.propertyFlags & property_flags) == property_flags) {
            return i;
        }
    }

    return -1;
}

static bool VulkanAllocateMemory(VulkanContext* context, VkDeviceSize size, i32 type_filter, VkMemoryPropertyFlags property_flags, VkMemoryAllocateFlags allocate_flags, VkDeviceMemory* out) {
    i32 memory_type_index = FindMemoryIndex(context->PhysicalDevice, type_filter, property_flags);
    if (memory_type_index == -1) {
        KERROR("[VulkanAllocateMemory]: Failed to find suitable memoryType");
        return false;
    }

    VkMemoryAllocateInfo info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    info.allocationSize = size;
    info.memoryTypeIndex = memory_type_index;

    if (allocate_flags) {
        VkMemoryAllocateFlagsInfo flags_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
        flags_info.flags = allocate_flags;
        info.pNext = &flags_info;
    }

    VkResult result = vkAllocateMemory(context->LogicalDevice.Handle, &info, context->AllocationCallbacks, out);
    KRAFT_VK_CHECK(result);

    return true;
}

void VulkanTempMemoryBlockAllocator::Initialize(ArenaAllocator* arena, u64 block_size) {
    this->block_size = block_size;
}

void VulkanTempMemoryBlockAllocator::Destroy() {}

void VulkanTempMemoryBlockAllocator::Clear() {
    if (current_block)
        while (current_block->prev)
            current_block = current_block->prev;

    current_free_offset = 0;
    allocation_count = 0;
}

BufferView VulkanTempMemoryBlockAllocator::Allocate(ArenaAllocator* arena, u64 size, u64 alignment) {
    if (!current_block) {
        current_block = ArenaPush(arena, TempMemoryBlock);
        current_block->block = AllocateGPUMemory(arena);
    }

    KASSERT(size <= this->block_size);
    u64 offset = kraft::math::AlignUp(this->current_free_offset, alignment);
    this->current_free_offset = offset + size;

    // We cannot fit the incoming request into this block, allocate a new block
    if (this->current_free_offset > this->block_size) {
        // Are there any previously allocated blocks ahead of this block?
        if (!current_block->next) {
            TempMemoryBlock* new_block = ArenaPush(arena, TempMemoryBlock);
            new_block->prev = current_block;
            current_block->next = new_block;
            current_block = new_block;
            current_block->block = AllocateGPUMemory(arena);
        }

        offset = 0;
        this->current_free_offset = size;
    }

    KASSERT(this->current_block->block.GPUBuffer);

    this->allocation_count++;
    return {
        .GPUBuffer = current_block->block.GPUBuffer,
        .Ptr = current_block->block.Ptr + offset,
        .Offset = offset,
    };
}

BufferView VulkanTempMemoryBlockAllocator::AllocateGPUMemory(ArenaAllocator* arena) {
    Handle<Buffer> buf = ResourceManager->CreateBuffer({
        .DebugName = "VkTempMemoryBlockAllocator",
        .Size = block_size,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_SRC | BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
        .MapMemory = true,
    });

    KASSERT(buf != Handle<Buffer>::Invalid());

    return BufferView{
        .GPUBuffer = buf,
        .Ptr = ResourceManager->GetBufferData(buf),
    };
}

static VkImageAspectFlags GetImageAspectMask(Format::Enum type) {
    if (type == Format::RGB8_UNORM || type == Format::RGBA8_UNORM || type == Format::BGRA8_UNORM || type == Format::BGR8_UNORM || type == Format::R32_SFLOAT) {
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (type == Format::D16_UNORM || type == Format::D32_SFLOAT) {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    if (type >= Format::D16_UNORM_S8_UINT) {
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    return VK_IMAGE_ASPECT_COLOR_BIT;
}

static VulkanResourceManagerState* state = nullptr;

static void Clear() {
    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;

    // We don't have to check if the handle to any resource is invalid here in any of the cleanup code
    // because vk___ functions don't do anything if the passed handle is "NULL"

    for (int i = 0; i < state->RenderPassPool.GetSize(); i++) {
        VulkanRenderPass& RenderPass = state->RenderPassPool.data[i];
        vkDeviceWaitIdle(device);

        vkDestroyFramebuffer(device, RenderPass.Framebuffer.Handle, context->AllocationCallbacks);
        vkDestroyRenderPass(device, RenderPass.Handle, context->AllocationCallbacks);

        RenderPass.Framebuffer.Handle = 0;
        RenderPass.Handle = 0;
    }

    for (int i = 0; i < state->texture_pool.GetSize(); i++) {
        VulkanTexture& texture = state->texture_pool.data[i];
        if (texture.View) {
            vkDestroyImageView(device, texture.View, context->AllocationCallbacks);
            texture.View = 0;
        }

        if (texture.Memory) {
            vkFreeMemory(device, texture.Memory, context->AllocationCallbacks);
            texture.Memory = 0;
        }

        if (texture.Image) {
            vkDestroyImage(device, texture.Image, context->AllocationCallbacks);
            texture.Image = 0;
        }
    }

    for (int i = 0; i < state->texture_sampler_pool.GetSize(); i++) {
        VulkanTextureSampler& sampler = state->texture_sampler_pool.data[i];

        vkDestroySampler(device, sampler.Sampler, context->AllocationCallbacks);
        sampler.Sampler = 0;
    }

    for (int i = 0; i < state->buffer_pool.GetSize(); i++) {
        VulkanBuffer& buffer = state->buffer_pool.data[i];
        vkFreeMemory(device, buffer.Memory, context->AllocationCallbacks);
        vkDestroyBuffer(device, buffer.Handle, context->AllocationCallbacks);

        buffer.Memory = 0;
        buffer.Handle = 0;
    }

    // We don't have to release command buffers individually, we can just delete the pool they were allocated from
    // for (int i = 0; i < state->CmdBufferPool.GetSize(); i++)
    // {
    //     CommandBuffer&       CmdBufferMetadata = state->CmdBufferPool.AuxiliaryData[i];
    //     VulkanCommandBuffer& CmdBuffer = state->CmdBufferPool.Data[i];

    //     vkFreeCommandBuffers(device, CmdBuffer.Pool, 1, &CmdBuffer.Resource);
    //     CmdBuffer.Resource = 0;
    //     CmdBuffer.Pool = 0;
    //     CmdBuffer.State = VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    // }

    for (int i = 0; i < state->cmd_pool_pool.GetSize(); i++) {
        VulkanCommandPool& cmd_pool = state->cmd_pool_pool.data[i];
        vkDestroyCommandPool(device, cmd_pool.Resource, context->AllocationCallbacks);
        cmd_pool.Resource = 0;
    }
}

static Handle<Texture> CreateTexture(const TextureDescription& description) {
    KASSERT(description.Dimensions.x > 0 && description.Dimensions.y > 0 && description.Dimensions.z > 0 && description.Dimensions.w > 0);

    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;
    VulkanTexture output = {};

    // Create a VkImage
    if (description.Tiling == TextureTiling::Linear) {
        KASSERT(description.Type == TextureType::Type2D);
        // TODO: Depth format restrictions check
    }

    VkImageCreateInfo create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
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
    if (description.Format >= Format::D16_UNORM && create_info.format != context->PhysicalDevice.DepthBufferFormat) {
        VkImageFormatProperties format_properties;
        VkResult result = vkGetPhysicalDeviceImageFormatProperties(
            context->PhysicalDevice.Handle, create_info.format, create_info.imageType, create_info.tiling, create_info.usage, create_info.flags, &format_properties
        );
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
            KWARN(
                "Supplied image format: %s is not supported by the GPU; Falling back to default "
                "depth buffer format.",
                Format::String(description.Format)
            );
            create_info.format = context->PhysicalDevice.DepthBufferFormat;
        }
    }

    // Ensure the image format is supported
    VkResult result = vkCreateImage(device, &create_info, context->AllocationCallbacks, &output.Image);
    if (result != VK_SUCCESS) {
        return Handle<Texture>::Invalid();
    }

    KRAFT_RENDERER_SET_OBJECT_NAME(output.Image, VK_OBJECT_TYPE_IMAGE, description.DebugName);

    VkMemoryRequirements memory_requirements = {};
    vkGetImageMemoryRequirements(device, output.Image, &memory_requirements);
    if (!VulkanAllocateMemory(context, memory_requirements.size, memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, &output.Memory)) {
        return Handle<Texture>::Invalid();
    }

    KRAFT_RENDERER_SET_OBJECT_NAME(output.Memory, VK_OBJECT_TYPE_DEVICE_MEMORY, description.DebugName);

    KRAFT_VK_CHECK(vkBindImageMemory(device, output.Image, output.Memory, 0));

    // Now create the image view
    VkImageViewCreateInfo view_create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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
    KRAFT_RENDERER_SET_OBJECT_NAME(output.View, VK_OBJECT_TYPE_IMAGE_VIEW, description.DebugName);

    Texture metadata = {
        .Width = description.Dimensions.x,
        .Height = description.Dimensions.y,
        .Channels = (u8)description.Dimensions.w,
        .TextureFormat = description.Format,
        .SampleCount = description.SampleCount,
    };

    StringCopy(metadata.DebugName, description.DebugName);

    return state->texture_pool.Insert(metadata, output);
}

static Handle<TextureSampler> CreateTextureSampler(const TextureSamplerDescription& description) {
    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;
    VulkanTextureSampler output = {};

    VkSamplerCreateInfo info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = ToVulkanFilter(description.MagFilter);
    info.minFilter = ToVulkanFilter(description.MinFilter);
    info.addressModeU = ToVulkanSamplerAddressMode(description.WrapModeU);
    info.addressModeV = ToVulkanSamplerAddressMode(description.WrapModeV);
    info.addressModeW = ToVulkanSamplerAddressMode(description.WrapModeW);
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.compareEnable = description.Compare > CompareOp::Never ? VK_TRUE : VK_FALSE;
    info.compareOp = ToVulkanCompareOp(description.Compare);
    info.mipmapMode = ToVulkanSamplerMipMapMode(description.MipMapMode);
    info.maxAnisotropy = description.AnisotropyEnabled ? 16.0f : 1.0f;
    info.anisotropyEnable = description.AnisotropyEnabled;
    info.unnormalizedCoordinates = VK_FALSE;

    KRAFT_VK_CHECK(vkCreateSampler(device, &info, context->AllocationCallbacks, &output.Sampler));
    KRAFT_RENDERER_SET_OBJECT_NAME(output.Sampler, VK_OBJECT_TYPE_SAMPLER, description.DebugName);

    TextureSampler metadata{};

    return state->texture_sampler_pool.Insert(metadata, output);
}

static Handle<Buffer> CreateBuffer(const BufferDescription& description) {
    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;
    VulkanBuffer output = {};
    VkMemoryPropertyFlags memory_properties = ToVulkanMemoryPropertyFlags(description.MemoryPropertyFlags);
    VkMemoryAllocateFlags allocate_flags = ToVulkanMemoryAllocateFlags(description.MemoryAllocationFlags);
    u64 actual_size = description.Size; // TODO: Fix

    if (description.UsageFlags & BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER) {
        actual_size = math::AlignUp(actual_size, context->PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment);
    } else if (description.UsageFlags & BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER) {
        actual_size = math::AlignUp(actual_size, context->PhysicalDevice.Properties.limits.minStorageBufferOffsetAlignment);
    }

    VkBufferCreateInfo create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    create_info.size = actual_size;
    create_info.sharingMode = ToVulkanSharingMode(description.SharingMode);
    create_info.usage = ToVulkanBufferUsage(description.UsageFlags);

    KRAFT_VK_CHECK(vkCreateBuffer(device, &create_info, context->AllocationCallbacks, &output.Handle));
    KRAFT_RENDERER_SET_OBJECT_NAME(output.Handle, VK_OBJECT_TYPE_BUFFER, description.DebugName);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, output.Handle, &memory_requirements);

    if (!VulkanAllocateMemory(context, memory_requirements.size, memory_requirements.memoryTypeBits, memory_properties, allocate_flags, &output.Memory)) {
        return Handle<Buffer>::Invalid();
    }

    if (description.BindMemory) {
        KRAFT_VK_CHECK(vkBindBufferMemory(device, output.Handle, output.Memory, 0));
    }

    // Get the device address if this buffer is to be used as a raw pointer in shaders
    if (description.MemoryPropertyFlags & BUFFER_USAGE_FLAGS_SHADER_DEVICE_ADDRESS) {
        VkBufferDeviceAddressInfo addr_info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        addr_info.buffer = output.Handle;
        output.device_address = vkGetBufferDeviceAddress(device, &addr_info);
    }

    void* ptr = nullptr;
    if (description.MapMemory) {
        vkMapMemory(device, output.Memory, 0, VK_WHOLE_SIZE, 0, &ptr);
    }

    return state->buffer_pool.Insert({.Size = actual_size, .Ptr = ptr}, output);
}

static Handle<RenderPass> CreateRenderPass(const RenderPassDescription& description) {
#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    return Handle<RenderPass>::Invalid();
#endif

    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;

    VkSubpassDescription subpasses[8];
    u32 subpasses_index = 0;
    VkAttachmentReference depth_attachment_ref;
    VkAttachmentReference color_attachment_refs[31] = {};
    u32 attachment_refs_index = 0;
    VkAttachmentDescription attachments[31 + 1] = {};
    VkImageView framebuffer_image_views[31 + 1] = {};
    u32 attachments_index = 0;

    bool has_depth_target = false;
    for (i32 i = 0; i < description.Layout.Subpasses.Size(); i++) {
        const RenderPassSubpass& subpass = description.Layout.Subpasses[i];
        subpasses[subpasses_index] = {};
        subpasses[subpasses_index].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        if (subpass.DepthTarget) {
            has_depth_target = true;
            depth_attachment_ref.attachment = (u32)description.ColorTargets.Size(); // Depth attachment will always be at the end
            depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            subpasses[subpasses_index].pDepthStencilAttachment = &depth_attachment_ref;
        }

        if (subpass.ColorTargetSlots.Size() > 0) {
            subpasses[subpasses_index].colorAttachmentCount = (u32)subpass.ColorTargetSlots.Size();
            subpasses[subpasses_index].pColorAttachments = color_attachment_refs + attachment_refs_index;
            for (int j = 0; j < subpass.ColorTargetSlots.Size(); j++) {
                color_attachment_refs[attachment_refs_index].attachment = subpass.ColorTargetSlots[j];
                color_attachment_refs[attachment_refs_index].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                attachment_refs_index++;
            }
        }

        subpasses_index++;
    }

    // Create color attachments
    for (i32 i = 0; i < description.ColorTargets.Size(); i++) {
        const ColorTarget& target = description.ColorTargets[i];
        Texture* texture = state->texture_pool.GetAuxiliaryData(target.Texture);
        VulkanTexture* vk_texture = state->texture_pool.Get(target.Texture);

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

    if (has_depth_target) {
        Texture* depth_texture = state->texture_pool.GetAuxiliaryData(description.DepthTarget.Texture);
        VulkanTexture* vk_depth_texture = state->texture_pool.Get(description.DepthTarget.Texture);

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

    VkRenderPassCreateInfo create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    create_info.attachmentCount = attachments_index;
    create_info.pAttachments = attachments;
    create_info.dependencyCount = 2;
    create_info.pDependencies = dependencies;
    create_info.subpassCount = subpasses_index;
    create_info.pSubpasses = subpasses;

    VulkanRenderPass out_render_pass;
    KRAFT_VK_CHECK(vkCreateRenderPass(device, &create_info, context->AllocationCallbacks, &out_render_pass.Handle));
    KRAFT_RENDERER_SET_OBJECT_NAME(out_render_pass.Handle, VK_OBJECT_TYPE_RENDER_PASS, description.DebugName);

    out_render_pass.Framebuffer.AttachmentCount = attachments_index;

    VkFramebufferCreateInfo info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.attachmentCount = attachments_index;
    info.pAttachments = framebuffer_image_views;
    info.width = (u32)description.Dimensions.x;
    info.height = (u32)description.Dimensions.y;
    info.renderPass = out_render_pass.Handle;
    info.layers = 1;

    KRAFT_VK_CHECK(vkCreateFramebuffer(device, &info, context->AllocationCallbacks, &out_render_pass.Framebuffer.Handle));
    KRAFT_RENDERER_SET_OBJECT_NAME(out_render_pass.Framebuffer.Handle, VK_OBJECT_TYPE_FRAMEBUFFER, description.DebugName);

    return state->RenderPassPool.Insert(
        {
            .Dimensions = description.Dimensions,
            .DepthTarget = description.DepthTarget,
            .ColorTargets = description.ColorTargets,
        },
        out_render_pass
    );
}

static Handle<CommandBuffer> CreateCommandBuffer(const CommandBufferDescription& description) {
    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;

    // If no command pool is specified, use the default one
    VulkanCommandPool* cmd_pool = {};
    if (description.CommandPool) {
        cmd_pool = state->cmd_pool_pool.Get(description.CommandPool);
    } else {
        cmd_pool = state->cmd_pool_pool.Get(context->GraphicsCommandPool);
    }

    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = cmd_pool->Resource;
    allocate_info.level = description.Primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocate_info.commandBufferCount = 1;

    VulkanCommandBuffer out = {};
    KRAFT_VK_CHECK(vkAllocateCommandBuffers(device, &allocate_info, &out.Resource));
    out.State = VULKAN_COMMAND_BUFFER_STATE_READY;
    out.Pool = cmd_pool->Resource;

    KRAFT_RENDERER_SET_OBJECT_NAME(out.Resource, VK_OBJECT_TYPE_COMMAND_BUFFER, description.DebugName);

    if (description.Begin) {
        VulkanBeginCommandBuffer(&out, description.SingleUse, description.RenderPassContinue, description.SimultaneousUse);
    }

    return state->cmd_buffer_pool.Insert({.Pool = description.CommandPool}, out);
}

static Handle<CommandPool> CreateCommandPool(const CommandPoolDescription& description) {
    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;
    VulkanCommandPool out = {};

    VkCommandPoolCreateInfo info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    info.queueFamilyIndex = description.QueueFamilyIndex;
    info.flags = ToVulkanCommandPoolCreateFlags(description.Flags);

    KRAFT_VK_CHECK(vkCreateCommandPool(device, &info, context->AllocationCallbacks, &out.Resource));

    KRAFT_RENDERER_SET_OBJECT_NAME(out.Resource, VK_OBJECT_TYPE_COMMAND_POOL, description.DebugName);

    return state->cmd_pool_pool.Insert({}, out);
}

static void DestroyTexture(Handle<Texture> handle) {
    state->texture_pool.MarkForDelete(handle);
}

static void DestroyBuffer(Handle<Buffer> handle) {
    state->buffer_pool.MarkForDelete(handle);
}

static void DestroyRenderPass(Handle<RenderPass> handle) {
    state->RenderPassPool.MarkForDelete(handle);
}

static void DestroyTextureSampler(Handle<TextureSampler> handle) {
    state->texture_sampler_pool.MarkForDelete(handle);
}

static void DestroyCommandBuffer(Handle<CommandBuffer> handle) {
    // TODO: Should single-use command buffers be destroyed immediately?
    state->cmd_buffer_pool.MarkForDelete(handle);
}

static void DestroyCommandPool(Handle<CommandPool> handle) {
    state->cmd_pool_pool.MarkForDelete(handle);
}

static u8* GetBufferData(Handle<Buffer> handle) {
    VulkanContext* context = VulkanRendererBackend::Context();
    VkDevice device = context->LogicalDevice.Handle;
    Buffer* buffer = state->buffer_pool.GetAuxiliaryData(handle);
    VulkanBuffer* gpu_buffer = state->buffer_pool.Get(handle);

    // Map the buffer first if it not mapped yet
    if (!buffer->Ptr) {
        KRAFT_VK_CHECK(vkMapMemory(device, gpu_buffer->Memory, 0, VK_WHOLE_SIZE, 0, &buffer->Ptr));
    }

    return (u8*)buffer->Ptr;
}

static BufferView CreateTempBuffer(u64 size) {
    return state->temp_gpu_allocator->Allocate(state->arena, size, 128);
}

static bool UploadTexture(Handle<Texture> texture, Handle<Buffer> buffer, u64 buffer_offset) {
    VulkanContext* context = VulkanRendererBackend::Context();
    Texture* metadata = state->texture_pool.GetAuxiliaryData(texture);
    KASSERT(metadata);

    VulkanTexture* gpu_texture = state->texture_pool.Get(texture);
    KASSERT(gpu_texture);

    VulkanBuffer* gpu_buffer = state->buffer_pool.Get(buffer);
    KASSERT(gpu_buffer);

    // Allocate a single use command buffer
    Handle<CommandBuffer> temp_cmd_buffer = CreateCommandBuffer({
        .CommandPool = context->GraphicsCommandPool,
        .Primary = true,
        .Begin = true,
        .SingleUse = true,
    });

    VulkanCommandBuffer* gpu_temp_cmd_buffer = state->cmd_buffer_pool.Get(temp_cmd_buffer);

    // Default image layout is undefined, so make it transfer dst optimal
    // This will change the image layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VulkanTransitionImageLayout(context, gpu_temp_cmd_buffer, gpu_texture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the image pixels from the staging buffer to the image using the temp command buffer
    VkBufferImageCopy region = {};
    region.bufferOffset = buffer_offset;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;

    region.imageExtent.width = (u32)metadata->Width;
    region.imageExtent.height = (u32)metadata->Height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(gpu_temp_cmd_buffer->Resource, gpu_buffer->Handle, gpu_texture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition the layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    // so the image turns into a suitable format for the shader to read
    VulkanTransitionImageLayout(context, gpu_temp_cmd_buffer, gpu_texture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // End our temp command buffer
    VulkanEndAndSubmitSingleUseCommandBuffer(context, gpu_temp_cmd_buffer->Pool, gpu_temp_cmd_buffer, context->LogicalDevice.GraphicsQueue);
    DestroyCommandBuffer(temp_cmd_buffer);

    return true;
}

static bool UploadBuffer(const UploadBufferDescription& description) {
    VulkanBuffer* src = state->buffer_pool.Get(description.SrcBuffer);
    if (!src) {
        return false;
    }

    VulkanBuffer* dst = state->buffer_pool.Get(description.DstBuffer);
    if (!dst) {
        return false;
    }

    VulkanContext* context = VulkanRendererBackend::Context();

    // Allocate a single use command buffer
    Handle<CommandBuffer> temp_cmd_buf = CreateCommandBuffer({
        .CommandPool = context->GraphicsCommandPool,
        .Primary = true,
        .Begin = true,
        .SingleUse = true,
    });

    VulkanCommandBuffer* gpu_temp_cmd_buf = state->cmd_buffer_pool.Get(temp_cmd_buf);

    VkBufferCopy buffer_copy_info = {};
    buffer_copy_info.size = description.SrcSize;
    buffer_copy_info.srcOffset = description.SrcOffset;
    buffer_copy_info.dstOffset = description.DstOffset;

    vkCmdCopyBuffer(gpu_temp_cmd_buf->Resource, src->Handle, dst->Handle, 1, &buffer_copy_info);
    VulkanEndAndSubmitSingleUseCommandBuffer(context, gpu_temp_cmd_buf->Pool, gpu_temp_cmd_buf, context->LogicalDevice.GraphicsQueue);
    DestroyCommandBuffer(temp_cmd_buf);

    return true;
}

static bool ReadTextureData(const ReadTextureDataDescription& description) {
    KASSERT(description.SrcTexture.IsInvalid() == false);
    KASSERT(description.OutBuffer);

    Texture* metadata = state->texture_pool.GetAuxiliaryData(description.SrcTexture);
    u32 width_to_read = description.Width == 0 ? (u32)metadata->Width : (u32)description.Width;
    u32 height_to_read = description.Height == 0 ? (u32)metadata->Height : (u32)description.Height;
    u32 staging_buffer_size = width_to_read * height_to_read * 4;
    BufferView staging_buffer = CreateTempBuffer(staging_buffer_size);

    if (description.OutBufferSize < staging_buffer_size) {
        return false;
    }

    VulkanContext* context = VulkanRendererBackend::Context();
    VulkanTexture* gpu_texture = state->texture_pool.Get(description.SrcTexture);
    VulkanBuffer* gpu_buffer = state->buffer_pool.Get(staging_buffer.GPUBuffer);

    Handle<CommandBuffer> temp_cmd_buf = description.CmdBuffer;
    VulkanCommandBuffer* gpu_temp_cmd_buf;
    if (description.CmdBuffer) {
        gpu_temp_cmd_buf = state->cmd_buffer_pool.Get(description.CmdBuffer);
    } else {
        // Allocate a single use command buffer
        temp_cmd_buf = CreateCommandBuffer({
            .CommandPool = context->GraphicsCommandPool,
            .Primary = true,
            .Begin = true,
            .SingleUse = true,
        });

        gpu_temp_cmd_buf = state->cmd_buffer_pool.Get(temp_cmd_buf);
    }

    // We are going to copy the image to a buffer, so convert it to a "Transfer optimal" format
    VulkanTransitionImageLayout(context, gpu_temp_cmd_buf, gpu_texture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    KASSERT(metadata);
    VkBufferImageCopy region = {
        .bufferOffset = staging_buffer.Offset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = GetImageAspectMask(metadata->TextureFormat),
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset =
            {
                .x = description.OffsetX,
                .y = description.OffsetY,
            },
        .imageExtent =
            {
                .width = width_to_read,
                .height = height_to_read,
                .depth = 1,
            },
    };

    vkCmdCopyImageToBuffer(gpu_temp_cmd_buf->Resource, gpu_texture->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, gpu_buffer->Handle, 1, &region);

    // Transition the image back to a "shader friendly" layout
    VulkanTransitionImageLayout(context, gpu_temp_cmd_buf, gpu_texture->Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VulkanEndAndSubmitSingleUseCommandBuffer(context, gpu_temp_cmd_buf->Pool, gpu_temp_cmd_buf, context->LogicalDevice.GraphicsQueue);
    DestroyCommandBuffer(temp_cmd_buf);

    MemCpy(description.OutBuffer, staging_buffer.Ptr, staging_buffer_size);

    return true;
}

static Texture* GetTextureMetadata(Handle<Texture> handle) {
    return state->texture_pool.GetAuxiliaryData(handle);
}

static RenderPass* GetRenderPassMetadata(Handle<RenderPass> handle) {
    return state->RenderPassPool.GetAuxiliaryData(handle);
}

static void StartFrame(u64 frame_number) {}

static void EndFrame(u64 frame_number) {
    // Render passes will be destroyed first because they may be referencing images
    state->RenderPassPool.Cleanup([](VulkanRenderPass* resource) {
        VulkanContext* context = VulkanRendererBackend::Context();
        VkDevice device = context->LogicalDevice.Handle;
        vkDeviceWaitIdle(device);

        vkDestroyFramebuffer(device, resource->Framebuffer.Handle, context->AllocationCallbacks);
        vkDestroyRenderPass(device, resource->Handle, context->AllocationCallbacks);

        resource->Framebuffer.Handle = 0;
        resource->Handle = 0;
    });

    state->texture_pool.Cleanup([](VulkanTexture* resource) {
        VulkanContext* context = VulkanRendererBackend::Context();
        VkDevice device = context->LogicalDevice.Handle;

        if (resource->View) {
            vkDestroyImageView(device, resource->View, context->AllocationCallbacks);
            resource->View = 0;
        }

        if (resource->Memory) {
            vkFreeMemory(device, resource->Memory, context->AllocationCallbacks);
            resource->Memory = 0;
        }

        if (resource->Image) {
            vkDestroyImage(device, resource->Image, context->AllocationCallbacks);
            resource->Image = 0;
        }
    });

    state->texture_sampler_pool.Cleanup([](VulkanTextureSampler* resource) {
        VulkanContext* context = VulkanRendererBackend::Context();
        VkDevice device = context->LogicalDevice.Handle;
        vkDestroySampler(device, resource->Sampler, context->AllocationCallbacks);
        resource->Sampler = 0;
    });

    state->buffer_pool.Cleanup([](VulkanBuffer* resource) {
        VulkanContext* context = VulkanRendererBackend::Context();
        VkDevice device = context->LogicalDevice.Handle;

        vkFreeMemory(device, resource->Memory, context->AllocationCallbacks);
        vkDestroyBuffer(device, resource->Handle, context->AllocationCallbacks);

        resource->Memory = 0;
        resource->Handle = 0;
    });

    state->cmd_buffer_pool.Cleanup([](VulkanCommandBuffer* resource) {
        VulkanContext* context = VulkanRendererBackend::Context();
        VkDevice device = context->LogicalDevice.Handle;
        VkCommandPool cmd_pool = resource->Pool;

        vkFreeCommandBuffers(device, cmd_pool, 1, &resource->Resource);
        resource->Resource = 0;
        resource->State = VULKAN_COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    });

    state->cmd_pool_pool.Cleanup([](VulkanCommandPool* resource) {
        VulkanContext* context = VulkanRendererBackend::Context();
        VkDevice device = context->LogicalDevice.Handle;

        vkDestroyCommandPool(device, resource->Resource, context->AllocationCallbacks);
        resource->Resource = 0;
    });

    state->temp_gpu_allocator->Clear();
}

VulkanTexture* VulkanResourceManagerApi::GetTexture(Handle<Texture> handle) {
    return state->texture_pool.Get(handle);
}

VulkanTextureSampler* VulkanResourceManagerApi::GetTextureSampler(Handle<TextureSampler> handle) {
    return state->texture_sampler_pool.Get(handle);
}

VulkanBuffer* VulkanResourceManagerApi::GetBuffer(Handle<Buffer> handle) {
    return state->buffer_pool.Get(handle);
}

VulkanRenderPass* VulkanResourceManagerApi::GetRenderPass(Handle<RenderPass> handle) {
    return state->RenderPassPool.Get(handle);
}

VulkanCommandBuffer* VulkanResourceManagerApi::GetCommandBuffer(Handle<CommandBuffer> handle) {
    return state->cmd_buffer_pool.Get(handle);
}

VulkanCommandPool* VulkanResourceManagerApi::GetCommandPool(Handle<CommandPool> handle) {
    return state->cmd_pool_pool.Get(handle);
}

struct ResourceManager* CreateVulkanResourceManager(ArenaAllocator* arena) {
    state = ArenaPush(arena, VulkanResourceManagerState);
    state->arena = arena;
    state->texture_pool.Grow(KRAFT_RENDERER__MAX_GLOBAL_TEXTURES);
    state->texture_sampler_pool.Grow(4);
    state->buffer_pool.Grow(1024);
    state->RenderPassPool.Grow(16);
    state->cmd_buffer_pool.Grow(16);
    state->cmd_pool_pool.Grow(1);
    state->temp_gpu_allocator = ArenaPush(arena, VulkanTempMemoryBlockAllocator);
    state->temp_gpu_allocator->Initialize(arena, KRAFT_SIZE_MB(128));

    struct ResourceManager* api = ArenaPush(arena, struct ResourceManager);
    api->Clear = Clear;
    api->CreateTexture = CreateTexture;
    api->CreateTextureSampler = CreateTextureSampler;
    api->CreateBuffer = CreateBuffer;
    api->CreateRenderPass = CreateRenderPass;
    api->CreateCommandBuffer = CreateCommandBuffer;
    api->CreateCommandPool = CreateCommandPool;
    api->DestroyTexture = DestroyTexture;
    api->DestroyTextureSampler = DestroyTextureSampler;
    api->DestroyBuffer = DestroyBuffer;
    api->DestroyRenderPass = DestroyRenderPass;
    api->DestroyCommandBuffer = DestroyCommandBuffer;
    api->DestroyCommandPool = DestroyCommandPool;
    api->GetBufferData = GetBufferData;
    api->CreateTempBuffer = CreateTempBuffer;
    api->UploadTexture = UploadTexture;
    api->UploadBuffer = UploadBuffer;
    api->ReadTextureData = ReadTextureData;
    api->GetTextureMetadata = GetTextureMetadata;
    api->GetRenderPassMetadata = GetRenderPassMetadata;
    api->StartFrame = StartFrame;
    api->EndFrame = EndFrame;

    return api;
}

void DestroyVulkanResourceManager(struct ResourceManager* ResourceManager) {
    state->cmd_pool_pool.Destroy();
    state->cmd_buffer_pool.Destroy();
    state->RenderPassPool.Destroy();
    state->buffer_pool.Destroy();
    state->texture_pool.Destroy();
    state->texture_sampler_pool.Destroy();
}
} // namespace kraft::r
