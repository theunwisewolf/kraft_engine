#pragma once

#include "core/kraft_core.h"

namespace kraft::renderer {

struct VulkanContext;
struct VulkanRenderPass;
struct VulkanFramebuffer;

void VulkanCreateFramebuffer(VulkanContext* Context, uint32 Width, uint32 Height, VulkanRenderPass* RenderPass, VkImageView* Attachments, uint32 AttachmentCount, VulkanFramebuffer* Out);
void VulkanDestroyFramebuffer(VulkanContext* Context, VulkanFramebuffer* Framebuffer);

}