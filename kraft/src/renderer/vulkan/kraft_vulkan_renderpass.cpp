#include "kraft_vulkan_renderpass.h"

#include <renderer/vulkan/kraft_vulkan_resource_manager.h>

namespace kraft::renderer
{

void VulkanCreateRenderPass(VulkanContext* context, Vec4f color, Vec4f rect, float32 depth, uint32 stencil, VulkanRenderPass* out, bool SwapchainTarget, const char* DebugName)
{
    out->Color   = color;
    out->Rect    = rect;
    out->Depth   = depth;
    out->Stencil = stencil;

    bool depthAttachmentRequired = (context->PhysicalDevice.DepthBufferFormat != VK_FORMAT_UNDEFINED);
    uint32 attachmentCount = 1;
    VkAttachmentDescription attachmentDescriptions[2];

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format          = context->Swapchain.ImageFormat.format;
    colorAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout     = SwapchainTarget ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorAttachment.flags           = 0;
    attachmentDescriptions[0]       = colorAttachment;

    VkAttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format          = context->PhysicalDevice.DepthBufferFormat;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR;
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

    int SubpassDependenciesCount = 0;
    VkSubpassDependency Dependencies[5];
    if (SwapchainTarget)
    {
        VkSubpassDependency ColorPassDependency = 
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        Dependencies[SubpassDependenciesCount++] = ColorPassDependency;

        // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#first-render-pass-writes-to-a-depth-attachment-second-render-pass-re-uses-the-same-depth-attachment
        VkSubpassDependency DepthPassDependency = 
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // Both stages might have access the depth-buffer, so need both in src/dstStageMask
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        Dependencies[SubpassDependenciesCount++] = DepthPassDependency;
    }
    else
    {
        VkSubpassDependency ColorPassDependency1 = 
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_NONE_KHR,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        };

        VkSubpassDependency ColorPassDependency2 = 
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        };

        VkSubpassDependency DepthPassDependency1;
        DepthPassDependency1.srcSubpass = VK_SUBPASS_EXTERNAL;
        DepthPassDependency1.dstSubpass = 0;
        DepthPassDependency1.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        DepthPassDependency1.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        DepthPassDependency1.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        DepthPassDependency1.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        DepthPassDependency1.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkSubpassDependency DepthPassDependency2;
        DepthPassDependency2.srcSubpass = 0;
        DepthPassDependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
        DepthPassDependency2.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        DepthPassDependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        DepthPassDependency2.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        DepthPassDependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        DepthPassDependency2.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        Dependencies[SubpassDependenciesCount++] = ColorPassDependency1;
        Dependencies[SubpassDependenciesCount++] = ColorPassDependency2;
        Dependencies[SubpassDependenciesCount++] = DepthPassDependency1;
        Dependencies[SubpassDependenciesCount++] = DepthPassDependency2;
    }

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = attachmentCount;
    createInfo.pAttachments = attachmentDescriptions;
    createInfo.dependencyCount = SubpassDependenciesCount;
    createInfo.pDependencies = Dependencies;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    KRAFT_VK_CHECK(vkCreateRenderPass(context->LogicalDevice.Handle, &createInfo, context->AllocationCallbacks, &out->Handle));
    context->SetObjectName((uint64)out->Handle, VK_OBJECT_TYPE_RENDER_PASS, DebugName);
}

void VulkanBeginRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass, VkFramebuffer frameBuffer, VkSubpassContents contents)
{
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.framebuffer = frameBuffer;
    info.renderPass = pass->Handle;
    info.renderArea.offset.x = (int32)pass->Rect.x;
    info.renderArea.offset.y = (int32)pass->Rect.y;
    info.renderArea.extent.width = (uint32)pass->Rect.z;
    info.renderArea.extent.height = (uint32)pass->Rect.w;
    
    VkClearValue clearValues[2];
    clearValues[0].color.float32[0] = pass->Color.r;
    clearValues[0].color.float32[1] = pass->Color.g;
    clearValues[0].color.float32[2] = pass->Color.b;
    clearValues[0].color.float32[3] = pass->Color.a;
    clearValues[1].depthStencil.depth = pass->Depth;
    clearValues[1].depthStencil.stencil = pass->Stencil;

    info.clearValueCount = 2;
    info.pClearValues = clearValues;

    vkCmdBeginRenderPass(commandBuffer->Resource, &info, contents);
    commandBuffer->State = VULKAN_COMMAND_BUFFER_STATE_IN_RENDER_PASS;
}

void VulkanBeginRenderPass(VulkanContext* Context, VulkanCommandBuffer* CmdBuffer, Handle<RenderPass> PassHandle, VkSubpassContents Contents)
{
    VulkanRenderPass* GPURenderPass = Context->ResourceManager->GetRenderPassPool().Get(PassHandle);
    RenderPass* RenderPass = Context->ResourceManager->GetRenderPassPool().GetAuxiliaryData(PassHandle);

    VkRenderPassBeginInfo BeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    BeginInfo.framebuffer = GPURenderPass->Framebuffer.Handle;
    BeginInfo.renderPass = GPURenderPass->Handle;
    BeginInfo.renderArea.offset.x = 0;
    BeginInfo.renderArea.offset.y = 0;
    BeginInfo.renderArea.extent.width = (uint32)RenderPass->Dimensions.x;
    BeginInfo.renderArea.extent.height = (uint32)RenderPass->Dimensions.y;

    VkClearValue ClearValues[32] = {};
    uint32 ColorValuesIndex = 0;
    for (int i = 0; i < RenderPass->ColorTargets.Size(); i++)
    {
        const ColorTarget& Target = RenderPass->ColorTargets[i];
        ClearValues[i].color.float32[0] = Target.ClearColor.r;
        ClearValues[i].color.float32[1] = Target.ClearColor.g;
        ClearValues[i].color.float32[2] = Target.ClearColor.b;
        ClearValues[i].color.float32[3] = Target.ClearColor.a;

        ColorValuesIndex++;
    }

    ClearValues[ColorValuesIndex].depthStencil.depth = RenderPass->DepthTarget.Depth;
    ClearValues[ColorValuesIndex].depthStencil.stencil = RenderPass->DepthTarget.Stencil;
    ColorValuesIndex++;

    BeginInfo.clearValueCount = ColorValuesIndex;
    BeginInfo.pClearValues = ClearValues;

    vkCmdBeginRenderPass(CmdBuffer->Resource, &BeginInfo, Contents);
    CmdBuffer->State = VULKAN_COMMAND_BUFFER_STATE_IN_RENDER_PASS;

    VkViewport Viewport = {};
    Viewport.width = RenderPass->Dimensions.x;
    Viewport.height = RenderPass->Dimensions.y;
    Viewport.maxDepth = 1.0f;

    vkCmdSetViewport(CmdBuffer->Resource, 0, 1, &Viewport);

    VkRect2D Scissor = {};
    Scissor.extent = { (uint32)RenderPass->Dimensions.x, (uint32)RenderPass->Dimensions.y };

    vkCmdSetScissor(CmdBuffer->Resource, 0, 1, &Scissor);
}

void VulkanEndRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass)
{
    vkCmdEndRenderPass(commandBuffer->Resource);
    commandBuffer->State = VULKAN_COMMAND_BUFFER_STATE_RECORDING;
}

void VulkanEndRenderPass(VulkanCommandBuffer* CmdBuffer)
{
    vkCmdEndRenderPass(CmdBuffer->Resource);
    CmdBuffer->State = VULKAN_COMMAND_BUFFER_STATE_RECORDING;
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