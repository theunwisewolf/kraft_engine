#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft::renderer
{

VkFormat ToVulkanFormat(ShaderDataType::Enum Format)
{
    static VkFormat Mapping[ShaderDataType::Count] = 
    {
        VK_FORMAT_R32_SFLOAT, 
        VK_FORMAT_R32G32_SFLOAT, 
        VK_FORMAT_R32G32B32_SFLOAT, 
        VK_FORMAT_R32G32B32A32_SFLOAT, 
        VK_FORMAT_R32G32B32A32_SFLOAT, 
        VK_FORMAT_R8_SINT, 
        VK_FORMAT_R8G8B8A8_SNORM, 
        VK_FORMAT_R8_UINT, 
        VK_FORMAT_R8G8B8A8_UINT, 
        VK_FORMAT_R16G16_SINT, 
        VK_FORMAT_R16G16_SNORM, 
        VK_FORMAT_R16G16B16A16_SINT, 
        VK_FORMAT_R16G16B16A16_SNORM, 
        VK_FORMAT_R32_UINT, 
        VK_FORMAT_R32G32_UINT, 
        VK_FORMAT_R32G32B32A32_UINT 
    };

    return Mapping[Format];
}

VkShaderStageFlagBits ToVulkanShaderStage(ShaderStage::Enum Stage)
{
    static VkShaderStageFlagBits Mapping[ShaderStage::Count] = 
    {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT
    };

    return Mapping[Stage];
}

VkDescriptorType ToVulkanResourceType(ResourceType::Enum Value)
{
    static VkDescriptorType Mapping[ResourceType::Count] = 
    {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    return Mapping[Value];
}

VkPolygonMode ToVulkanPolygonMode(PolygonMode::Enum Value)
{
    static VkPolygonMode Mapping[PolygonMode::Count] = 
    {
        VK_POLYGON_MODE_FILL, 
        VK_POLYGON_MODE_LINE,
        VK_POLYGON_MODE_POINT
    };

    return Mapping[Value];
}

VkBlendFactor ToVulkanBlendFactor(BlendFactor::Enum Value)
{
    static VkBlendFactor Mapping[BlendFactor::Count] = 
    {
        VK_BLEND_FACTOR_ZERO, 
        VK_BLEND_FACTOR_ONE, 
        VK_BLEND_FACTOR_SRC_COLOR, 
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, 
        VK_BLEND_FACTOR_DST_COLOR, 
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
        VK_BLEND_FACTOR_SRC_ALPHA, 
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, 
        VK_BLEND_FACTOR_DST_ALPHA, 
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA
    };

    return Mapping[Value];
}

VkBlendOp ToVulkanBlendOp(BlendOp::Enum Value)
{
    static VkBlendOp Mapping[BlendOp::Count] = 
    {
        VK_BLEND_OP_ADD, 
        VK_BLEND_OP_SUBTRACT, 
        VK_BLEND_OP_REVERSE_SUBTRACT, 
        VK_BLEND_OP_MIN, 
        VK_BLEND_OP_MAX
    };

    return Mapping[Value];
}

VkCullModeFlagBits ToVulkanCullMode(CullMode::Enum Value)
{
    static VkCullModeFlagBits Mapping[CullMode::Count] = 
    {
        VK_CULL_MODE_NONE, 
        VK_CULL_MODE_FRONT_BIT, 
        VK_CULL_MODE_BACK_BIT, 
        VK_CULL_MODE_FRONT_AND_BACK
    };

    return Mapping[Value];
}

VkCompareOp ToVulkanCompareOp(CompareOp::Enum Value)
{
    static VkCompareOp Mapping[CompareOp::Count] = 
    {
        VK_COMPARE_OP_NEVER, 
        VK_COMPARE_OP_LESS, 
        VK_COMPARE_OP_EQUAL, 
        VK_COMPARE_OP_LESS_OR_EQUAL, 
        VK_COMPARE_OP_GREATER, 
        VK_COMPARE_OP_NOT_EQUAL, 
        VK_COMPARE_OP_GREATER_OR_EQUAL, 
        VK_COMPARE_OP_ALWAYS
    };

    return Mapping[Value];
}

}