#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

void VulkanCreateFramebuffer(VulkanContext* Context, uint32 Width, uint32 Height, VulkanRenderPass* RenderPass, VkImageView* Attachments, uint32 AttachmentCount, VulkanFramebuffer* Out);
void VulkanDestroyFramebuffer(VulkanContext* Context, VulkanFramebuffer* Framebuffer);

}