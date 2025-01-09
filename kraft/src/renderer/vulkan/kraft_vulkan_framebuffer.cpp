#include <volk/volk.h>

#include "kraft_vulkan_framebuffer.h"

#include <containers/kraft_carray.h>
#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_memory.h>

#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::renderer {

void VulkanCreateFramebuffer(VulkanContext* Context, uint32 Width, uint32 Height, VulkanRenderPass* RenderPass, VkImageView* Attachments, uint32 AttachmentCount, VulkanFramebuffer* Out)
{
    Out->AttachmentCount = AttachmentCount;
    CreateArray(Out->Attachments, Out->AttachmentCount);
    for (uint32 i = 0; i < Out->AttachmentCount; ++i)
    {
        Out->Attachments[i] = Attachments[i];
    }

    Out->Width = Width;
    Out->Height = Height;

    VkFramebufferCreateInfo Info = {};
    Info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    Info.attachmentCount = Out->AttachmentCount;
    Info.pAttachments = Out->Attachments;
    Info.width = Width;
    Info.height = Height;
    Info.renderPass = RenderPass->Handle;
    Info.layers = 1;

    KRAFT_VK_CHECK(vkCreateFramebuffer(Context->LogicalDevice.Handle, &Info, Context->AllocationCallbacks, &Out->Handle));
}

void VulkanDestroyFramebuffer(VulkanContext* Context, VulkanFramebuffer* Framebuffer)
{
    vkDestroyFramebuffer(Context->LogicalDevice.Handle, Framebuffer->Handle, Context->AllocationCallbacks);
    DestroyArray(Framebuffer->Attachments);

    Framebuffer->AttachmentCount = 0;
    Framebuffer->Handle = 0;
    Framebuffer->Width = 0;
    Framebuffer->Height = 0;
}

}