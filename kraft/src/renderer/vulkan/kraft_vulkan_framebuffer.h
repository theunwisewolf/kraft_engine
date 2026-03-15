#pragma once

#include "core/kraft_core.h"

namespace kraft::r {

struct VulkanContext;
struct VulkanRenderPass;
struct VulkanFramebuffer;

void VulkanCreateFramebuffer(VulkanContext* Context, u32 Width, u32 Height, VulkanRenderPass* RenderPass, VkImageView* Attachments, u32 AttachmentCount, VulkanFramebuffer* Out);
void VulkanDestroyFramebuffer(VulkanContext* Context, VulkanFramebuffer* Framebuffer);

}