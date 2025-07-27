#pragma once

#ifndef VOLK_H_
#error "Vulkan headers not found. Did you forget to include volk.h before this file?"
#endif

#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::renderer {

static inline VkFormat ToVulkanFormat(ShaderDataType Format)
{
    static VkFormat Mapping[ShaderDataType::Count] = { VK_FORMAT_R32_SFLOAT,          VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                       VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8_SINT,       VK_FORMAT_R8G8B8A8_SNORM,   VK_FORMAT_R8_UINT,
                                                       VK_FORMAT_R8G8B8A8_UINT,       VK_FORMAT_R16G16_SINT,   VK_FORMAT_R16G16_SNORM,     VK_FORMAT_R16G16B16A16_SINT,
                                                       VK_FORMAT_R16G16B16A16_SNORM,  VK_FORMAT_R32_UINT,      VK_FORMAT_R32G32_UINT,      VK_FORMAT_R32G32B32A32_UINT };

    return Mapping[Format.UnderlyingType];
}

static inline VkFormat ToVulkanFormat(Format::Enum Format)
{
    static VkFormat Mapping[Format::Count] = {
        // Color formats
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,

        // Depth-Stencil formats
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    return Mapping[Format];
}

static inline VkShaderStageFlagBits ToVulkanShaderStageFlagBits(ShaderStageFlags Value)
{
    if (Value & ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX)
        return VK_SHADER_STAGE_VERTEX_BIT;
    if (Value & ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT)
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    if (Value & ShaderStageFlags::SHADER_STAGE_FLAGS_COMPUTE)
        return VK_SHADER_STAGE_COMPUTE_BIT;
    if (Value & ShaderStageFlags::SHADER_STAGE_FLAGS_GEOMETRY)
        return VK_SHADER_STAGE_GEOMETRY_BIT;

    KASSERTM(false, "Invalid shader stage");
    return VkShaderStageFlagBits(0);
}

static inline VkShaderStageFlags ToVulkanShaderStageFlags(uint64 Flags)
{
    VkShaderStageFlags Result = 0;

    Result |= (Flags & SHADER_STAGE_FLAGS_VERTEX) ? VK_SHADER_STAGE_VERTEX_BIT : 0;
    Result |= (Flags & SHADER_STAGE_FLAGS_GEOMETRY) ? VK_SHADER_STAGE_GEOMETRY_BIT : 0;
    Result |= (Flags & SHADER_STAGE_FLAGS_FRAGMENT) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0;
    Result |= (Flags & SHADER_STAGE_FLAGS_COMPUTE) ? VK_SHADER_STAGE_COMPUTE_BIT : 0;

    return Result;
}

static inline VkDescriptorType ToVulkanResourceType(ResourceType::Enum Value)
{
    static VkDescriptorType Mapping[ResourceType::Count] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };

    return Mapping[Value];
}

static inline VkPolygonMode ToVulkanPolygonMode(PolygonMode::Enum Value)
{
    static VkPolygonMode Mapping[PolygonMode::Count] = { VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE, VK_POLYGON_MODE_POINT };

    return Mapping[Value];
}

static inline VkBlendFactor ToVulkanBlendFactor(BlendFactor::Enum Value)
{
    static VkBlendFactor Mapping[BlendFactor::Count] = { VK_BLEND_FACTOR_ZERO,      VK_BLEND_FACTOR_ONE,
                                                         VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                                                         VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
                                                         VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                         VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA };

    return Mapping[Value];
}

static inline VkBlendOp ToVulkanBlendOp(BlendOp::Enum Value)
{
    static VkBlendOp Mapping[BlendOp::Count] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };

    return Mapping[Value];
}

static inline VkCullModeFlagBits ToVulkanCullModeFlagBits(CullModeFlags::Enum Value)
{
    static VkCullModeFlagBits Mapping[CullModeFlags::Count] = {
        VK_CULL_MODE_NONE,
        VK_CULL_MODE_FRONT_BIT,
        VK_CULL_MODE_BACK_BIT,
        VK_CULL_MODE_FRONT_AND_BACK,
    };

    return Mapping[Value];
}

static inline VkCullModeFlags ToVulkanCullModeFlags(uint64 Flags)
{
    VkCullModeFlags Result = 0;

    Result |= (Flags & CullModeFlags::None) ? VK_CULL_MODE_NONE : 0;
    Result |= (Flags & CullModeFlags::Front) ? VK_CULL_MODE_FRONT_BIT : 0;
    Result |= (Flags & CullModeFlags::Back) ? VK_CULL_MODE_BACK_BIT : 0;
    Result |= (Flags & CullModeFlags::FrontAndBack) ? VK_CULL_MODE_FRONT_AND_BACK : 0;

    return Result;
}

static inline VkCompareOp ToVulkanCompareOp(CompareOp::Enum Value)
{
    static VkCompareOp Mapping[CompareOp::Count] = {
        VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS
    };

    return Mapping[Value];
}

// VkImageUsageFlagBits ToVulkanImageUsageFlagBits(TextureUsageFlags::Enum Value)
// {
//     static VkImageUsageFlagBits Mapping[TextureUsageFlags::Count] =
//     {
//         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
//         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//         VK_IMAGE_USAGE_SAMPLED_BIT,
//         VK_IMAGE_USAGE_STORAGE_BIT,
//         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
//         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
//     };

//     return Mapping[Value];
// }

static inline VkImageUsageFlags ToVulkanImageUsageFlags(uint64 Flags)
{
    VkImageUsageFlags Result = 0;

    Result |= (Flags & TEXTURE_USAGE_FLAGS_TRANSFER_SRC) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
    Result |= (Flags & TEXTURE_USAGE_FLAGS_TRANSFER_DST) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;
    Result |= (Flags & TEXTURE_USAGE_FLAGS_SAMPLED) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
    Result |= (Flags & TEXTURE_USAGE_FLAGS_STORAGE) ? VK_IMAGE_USAGE_STORAGE_BIT : 0;
    Result |= (Flags & TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0;
    Result |= (Flags & TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
    Result |= (Flags & TEXTURE_USAGE_FLAGS_TRANSIENT_ATTACHMENT) ? VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT : 0;

    return Result;
}

static inline VkImageTiling ToVulkanImageTiling(TextureTiling::Enum Value)
{
    static VkImageTiling Mapping[TextureTiling::Count] = { VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TILING_LINEAR };

    return Mapping[Value];
}

static inline VkImageType ToVulkanImageType(TextureType::Enum Value)
{
    static VkImageType Mapping[TextureType::Count] = { VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D };

    return Mapping[Value];
}

static inline VkFilter ToVulkanFilter(TextureFilter::Enum Value)
{
    static VkFilter Mapping[] = {
        VK_FILTER_NEAREST,
        VK_FILTER_LINEAR,
    };

    return Mapping[Value];
}

static inline VkSamplerAddressMode ToVulkanSamplerAddressMode(TextureWrapMode::Enum Value)
{
    static VkSamplerAddressMode Mapping[] = {
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    };

    return Mapping[Value];
}

static inline VkSampleCountFlagBits ToVulkanSampleCountFlagBits(TextureSampleCountFlags Value)
{
    if (Value & TextureSampleCountFlags::TEXTURE_SAMPLE_COUNT_FLAGS_1)
    {
        return VK_SAMPLE_COUNT_1_BIT;
    }

    if (Value & TextureSampleCountFlags::TEXTURE_SAMPLE_COUNT_FLAGS_2)
    {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    if (Value & TextureSampleCountFlags::TEXTURE_SAMPLE_COUNT_FLAGS_4)
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }

    if (Value & TextureSampleCountFlags::TEXTURE_SAMPLE_COUNT_FLAGS_8)
    {
        return VK_SAMPLE_COUNT_8_BIT;
    }

    if (Value & TextureSampleCountFlags::TEXTURE_SAMPLE_COUNT_FLAGS_16)
    {
        return VK_SAMPLE_COUNT_16_BIT;
    }

    if (Value & TextureSampleCountFlags::TEXTURE_SAMPLE_COUNT_FLAGS_32)
    {
        return VK_SAMPLE_COUNT_32_BIT;
    }

    if (Value & TextureSampleCountFlags::TEXTURE_SAMPLE_COUNT_FLAGS_64)
    {
        return VK_SAMPLE_COUNT_64_BIT;
    }

    KASSERTM(false, "Invalid sample count");
    return VkSampleCountFlagBits(0);
}

static inline VkSampleCountFlags ToVulkanSampleCountFlags(uint64 Flags)
{
    VkSampleCountFlags Result = 0;

    Result |= (Flags & TEXTURE_SAMPLE_COUNT_FLAGS_1) ? VK_SAMPLE_COUNT_1_BIT : 0;
    Result |= (Flags & TEXTURE_SAMPLE_COUNT_FLAGS_2) ? VK_SAMPLE_COUNT_2_BIT : 0;
    Result |= (Flags & TEXTURE_SAMPLE_COUNT_FLAGS_4) ? VK_SAMPLE_COUNT_4_BIT : 0;
    Result |= (Flags & TEXTURE_SAMPLE_COUNT_FLAGS_8) ? VK_SAMPLE_COUNT_8_BIT : 0;
    Result |= (Flags & TEXTURE_SAMPLE_COUNT_FLAGS_16) ? VK_SAMPLE_COUNT_16_BIT : 0;
    Result |= (Flags & TEXTURE_SAMPLE_COUNT_FLAGS_32) ? VK_SAMPLE_COUNT_32_BIT : 0;
    Result |= (Flags & TEXTURE_SAMPLE_COUNT_FLAGS_64) ? VK_SAMPLE_COUNT_64_BIT : 0;

    return Result;
}

static inline VkSamplerMipmapMode ToVulkanSamplerMipMapMode(TextureMipMapMode::Enum Value)
{
    static VkSamplerMipmapMode Mapping[] = {
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };

    return Mapping[Value];
}

static inline VkSharingMode ToVulkanSharingMode(SharingMode::Enum Value)
{
    static VkSharingMode Mapping[SharingMode::Count] = {
        VK_SHARING_MODE_EXCLUSIVE,
        VK_SHARING_MODE_CONCURRENT,
    };

    return Mapping[Value];
}

// VkBufferUsageFlagBits ToVulkanBufferUsageFlagBits(BufferUsageFlags::Enum Value)
// {
//     static VkBufferUsageFlagBits Mapping[BufferUsageFlags::Count] =
//     {
//         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
//         VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
//         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
//         VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
//         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
//     };

//     return Mapping[Value];
// }

static inline VkBufferUsageFlags ToVulkanBufferUsage(uint64 Flags)
{
    VkBufferUsageFlags Result = 0;

    Result |= (Flags & BUFFER_USAGE_FLAGS_TRANSFER_SRC) ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_TRANSFER_DST) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_UNIFORM_TEXEL_BUFFER) ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_STORAGE_TEXEL_BUFFER) ? VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_UNIFORM_BUFFER) ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_STORAGE_BUFFER) ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_INDEX_BUFFER) ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_VERTEX_BUFFER) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 0;
    Result |= (Flags & BUFFER_USAGE_FLAGS_INDEX_BUFFER) ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT : 0;

    return Result;
}

// VkMemoryPropertyFlagBits ToVulkanMemoryPropertyFlagBits(MemoryPropertyFlags::Enum Value)
// {
//     static VkMemoryPropertyFlagBits Mapping[MemoryPropertyFlags::Count] =
//     {
//         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
//         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
//     };

//     return Mapping[Value];
// }

static inline VkMemoryPropertyFlags ToVulkanMemoryPropertyFlags(uint64 Flags)
{
    VkMemoryPropertyFlags Result = 0;

    Result |= (Flags & MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL) ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
    Result |= (Flags & MEMORY_PROPERTY_FLAGS_HOST_VISIBLE) ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0;
    Result |= (Flags & MEMORY_PROPERTY_FLAGS_HOST_COHERENT) ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0;
    Result |= (Flags & MEMORY_PROPERTY_FLAGS_HOST_CACHED) ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : 0;

    return Result;
}

static inline VkAttachmentLoadOp ToVulkanAttachmentLoadOp(LoadOp::Enum Value)
{
    static VkAttachmentLoadOp Mapping[] = {
        VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD,
        VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR,
        VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    };

    return Mapping[Value];
}

static inline VkAttachmentStoreOp ToVulkanAttachmentStoreOp(StoreOp::Enum Value)
{
    static VkAttachmentStoreOp Mapping[] = {
        VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE,
        VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE,
    };

    return Mapping[Value];
}

static inline VkImageLayout ToVulkanImageLayout(TextureLayout::Enum Value)
{
    static VkImageLayout Mapping[] = {
        VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,       VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };

    return Mapping[Value];
}

static inline VkCommandPoolCreateFlagBits ToVulkanCommandPoolCreateFlagBits(CommandPoolCreateFlags Flag)
{
    // We have a 1-1 mapping to the vulkan type
    return (VkCommandPoolCreateFlagBits)Flag;
}

static inline VkCommandPoolCreateFlags ToVulkanCommandPoolCreateFlags(int32 Flags)
{
    VkCommandPoolCreateFlags Result = 0;
    Result |= (Flags & COMMAND_POOL_CREATE_FLAGS_TRANSIENT_BIT) ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;
    Result |= (Flags & COMMAND_POOL_CREATE_FLAGS_RESET_COMMAND_BUFFER_BIT) ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;
    Result |= (Flags & COMMAND_POOL_CREATE_FLAGS_PROTECTED_BIT) ? VK_COMMAND_POOL_CREATE_PROTECTED_BIT : 0;

    return Result;
}

}