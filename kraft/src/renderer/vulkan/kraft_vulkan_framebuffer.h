#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

void VulkanCreateFramebuffer(VulkanContext* context, uint32 width, uint32 height, VulkanRenderPass* renderPass, VkImageView* attachments, VulkanFramebuffer* out);
void VulkanDestroyFramebuffer(VulkanContext* context, VulkanFramebuffer* framebuffer);

}