#include "kraft_vulkan_framebuffer.h"

#include "core/kraft_memory.h"
#include "containers/array.h"

namespace kraft::renderer
{

void VulkanCreateFramebuffer(VulkanContext* context, uint32 width, uint32 height, VulkanRenderPass* renderPass, VkImageView* attachments, VulkanFramebuffer* out)
{
    out->AttachmentCount = (uint32)arrlen(attachments);
    arrsetlen(out->Attachments, out->AttachmentCount);
    for (uint32 i = 0; i < out->AttachmentCount; ++i)
    {
        out->Attachments[i] = attachments[i];
    }

    out->RenderPass = renderPass;
    out->Width = width;
    out->Height = height;

    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.attachmentCount = out->AttachmentCount;
    info.pAttachments = out->Attachments;
    info.width = width;
    info.height = height;
    info.renderPass = renderPass->Handle;
    info.layers = 1;

    KRAFT_VK_CHECK(vkCreateFramebuffer(context->LogicalDevice.Handle, &info, context->AllocationCallbacks, &out->Handle));
}

void VulkanDestroyFramebuffer(VulkanContext* context, VulkanFramebuffer* framebuffer)
{
    vkDestroyFramebuffer(context->LogicalDevice.Handle, framebuffer->Handle, context->AllocationCallbacks);
    arrfree(framebuffer->Attachments);

    framebuffer->AttachmentCount = 0;
    framebuffer->Handle = 0;
    framebuffer->RenderPass = 0;
    framebuffer->Width = 0;
    framebuffer->Height = 0;
}

}