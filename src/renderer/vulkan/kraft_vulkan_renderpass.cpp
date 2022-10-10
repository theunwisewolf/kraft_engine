#include "kraft_vulkan_renderpass.h"

namespace kraft
{

void VulkanCreateRenderPass(VulkanContext* context, float4 color, float4 rect, float32 depth, uint32 stencil, VulkanRenderPass* out)
{
    out->Color   = color;
    out->Rect    = rect;
    out->Depth   = depth;
    out->Stencil = stencil;

    bool depthAttachmentRequired = (context->PhysicalDevice.DepthBufferFormat != VK_FORMAT_UNDEFINED);
    uint32 attachmentCount = 1;
    VkAttachmentDescription attachmentDescriptions[2];

    VkAttachmentDescription colorAttachment;
    colorAttachment.format          = context->Swapchain.ImageFormat.format;
    colorAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.flags           = 0;
    attachmentDescriptions[0]       = colorAttachment;

    VkAttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment;
    depthAttachment.format          = context->PhysicalDevice.DepthBufferFormat;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.flags           = 0;

    VkAttachmentReference depthAttachmentRef;
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    if (depthAttachmentRequired)
    {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        attachmentDescriptions[1]       = depthAttachment;
        attachmentCount++;
    }

    VkSubpassDependency subpassDependency;
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependency.dependencyFlags = 0;

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = attachmentCount;
    createInfo.pAttachments = attachmentDescriptions;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &subpassDependency;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    KRAFT_VK_CHECK(vkCreateRenderPass(context->LogicalDevice.Handle, &createInfo, context->AllocationCallbacks, &out->Handle));
}

void VulkanBeginRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass, VkFramebuffer frameBuffer)
{
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.framebuffer = frameBuffer;
    info.renderPass = pass->Handle;
    info.renderArea.offset.x = pass->Rect.x;
    info.renderArea.offset.y = pass->Rect.y;
    info.renderArea.extent.width = pass->Rect.z;
    info.renderArea.extent.height = pass->Rect.w;
    
    VkClearValue clearValues[2];
    clearValues[0].color.float32[0] = pass->Color.r;
    clearValues[0].color.float32[1] = pass->Color.g;
    clearValues[0].color.float32[2] = pass->Color.b;
    clearValues[0].color.float32[3] = pass->Color.a;
    clearValues[1].depthStencil.depth = pass->Depth;
    clearValues[1].depthStencil.stencil = pass->Stencil;

    info.clearValueCount = 2;
    info.pClearValues = clearValues;

    vkCmdBeginRenderPass(commandBuffer->Handle, &info, VK_SUBPASS_CONTENTS_INLINE);
    commandBuffer->State = VULKAN_COMMAND_BUFFER_STATE_IN_RENDER_PASS;
}

void VulkanEndRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass)
{
    vkCmdEndRenderPass(commandBuffer->Handle);
    commandBuffer->State = VULKAN_COMMAND_BUFFER_STATE_RECORDING;
}

void VulkanDestroyRenderPass(VulkanContext* context, VulkanRenderPass* pass)
{
    if (pass && pass->Handle)
    {
        vkDestroyRenderPass(context->LogicalDevice.Handle, pass->Handle, context->AllocationCallbacks);
        pass->Handle = 0;
    }
}

}