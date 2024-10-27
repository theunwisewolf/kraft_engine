#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft::renderer
{

VkFormat ToVulkanFormat(ShaderDataType::Enum Format);;
VkFormat ToVulkanFormat(Format::Enum Format);;
VkShaderStageFlagBits ToVulkanShaderStageFlagBits(ShaderStageFlags Stage);
VkShaderStageFlags ToVulkanShaderStageFlags(uint64 Flags);
VkDescriptorType ToVulkanResourceType(ResourceType::Enum Value);
VkPolygonMode ToVulkanPolygonMode(PolygonMode::Enum Value);
VkBlendFactor ToVulkanBlendFactor(BlendFactor::Enum Value);
VkBlendOp ToVulkanBlendOp(BlendOp::Enum Value);
VkCullModeFlagBits ToVulkanCullModeFlagBits(CullModeFlags::Enum Value);
VkCullModeFlags ToVulkanCullModeFlags(uint64 Flags);
VkCompareOp ToVulkanCompareOp(CompareOp::Enum Value);
// VkImageUsageFlagBits ToVulkanImageUsageFlagBits(TextureUsageFlags::Enum Value);
VkImageUsageFlags ToVulkanImageUsageFlags(uint64 Flags);
VkImageTiling ToVulkanImageTiling(TextureTiling::Enum Value);
VkImageType ToVulkanImageType(TextureType::Enum Value);
VkFilter ToVulkanFilter(TextureFilter::Enum Value);
VkSamplerAddressMode ToVulkanSamplerAddressMode(TextureWrapMode::Enum Value);
VkSampleCountFlagBits ToVulkanSampleCountFlagBits(TextureSampleCountFlags Value);
VkSamplerMipmapMode ToVulkanSamplerMipMapMode(TextureMipMapMode::Enum Value);
VkSampleCountFlags ToVulkanSampleCountFlags(uint64 Flags);
VkSharingMode ToVulkanSharingMode(SharingMode::Enum Value);
// VkBufferUsageFlagBits ToVulkanBufferUsageFlagBits(BufferUsageFlags::Enum Value);
VkBufferUsageFlags ToVulkanBufferUsage(uint64 Flags);
// VkMemoryPropertyFlagBits ToVulkanMemoryPropertyFlagBits(MemoryPropertyFlags::Enum Value);
VkMemoryPropertyFlags ToVulkanMemoryPropertyFlags(uint64 Flags);
VkAttachmentLoadOp ToVulkanAttachmentLoadOp(LoadOp::Enum Op);
VkAttachmentStoreOp ToVulkanAttachmentStoreOp(StoreOp::Enum Op);
VkImageLayout ToVulkanImageLayout(TextureLayout::Enum Layout);

}