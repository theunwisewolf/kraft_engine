#include <volk/volk.h>

#include "kraft_includes.h"

namespace kraft::r {

void VulkanCreateRenderPass(
    VulkanContext*    context,
    vec4              color,
    vec4              rect,
    f32               depth,
    u32               stencil,
    VulkanRenderPass* out,
    bool              swapchain_target,
    const char*       debug_name
)
{
    out->Color = color;
    out->Rect = rect;
    out->Depth = depth;
    out->Stencil = stencil;

    bool depth_attachment_required = (context->PhysicalDevice.DepthBufferFormat != VK_FORMAT_UNDEFINED);
    u32  attachmentCount = 1;
    VkAttachmentDescription attachment_descriptions[2] = {};

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = context->Swapchain.ImageFormat.format;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout =
        swapchain_target ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    color_attachment.flags = 0;
    attachment_descriptions[0] = color_attachment;

    VkAttachmentReference colorAttachmentRef;
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = context->PhysicalDevice.DepthBufferFormat;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment.flags = 0;

    VkAttachmentReference depth_attachment_ref;
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    if (depth_attachment_required)
    {
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
        attachment_descriptions[1] = depth_attachment;
        attachmentCount++;
    }

    int                 subpass_dependencies_count = 0;
    VkSubpassDependency dependencies[5];
    if (swapchain_target)
    {
        VkSubpassDependency color_pass_dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        dependencies[subpass_dependencies_count++] = color_pass_dependency;

        // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#first-render-pass-writes-to-a-depth-attachment-second-render-pass-re-uses-the-same-depth-attachment
        VkSubpassDependency depth_pass_dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask =
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // Both stages might have access the depth-buffer, so need both in src/dstStageMask
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        dependencies[subpass_dependencies_count++] = depth_pass_dependency;
    }
    else
    {
        VkSubpassDependency color_pass_dependency_1 = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_NONE_KHR,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        };

        VkSubpassDependency color_passdependency_2 = {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        };

        VkSubpassDependency depth_pass_dependency_1;
        depth_pass_dependency_1.srcSubpass = VK_SUBPASS_EXTERNAL;
        depth_pass_dependency_1.dstSubpass = 0;
        depth_pass_dependency_1.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depth_pass_dependency_1.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        depth_pass_dependency_1.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        depth_pass_dependency_1.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depth_pass_dependency_1.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkSubpassDependency depth_pass_dependency_2;
        depth_pass_dependency_2.srcSubpass = 0;
        depth_pass_dependency_2.dstSubpass = VK_SUBPASS_EXTERNAL;
        depth_pass_dependency_2.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        depth_pass_dependency_2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        depth_pass_dependency_2.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depth_pass_dependency_2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        depth_pass_dependency_2.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[subpass_dependencies_count++] = color_pass_dependency_1;
        dependencies[subpass_dependencies_count++] = color_passdependency_2;
        dependencies[subpass_dependencies_count++] = depth_pass_dependency_1;
        dependencies[subpass_dependencies_count++] = depth_pass_dependency_2;
    }

    VkRenderPassCreateInfo create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    create_info.attachmentCount = attachmentCount;
    create_info.pAttachments = attachment_descriptions;
    create_info.dependencyCount = subpass_dependencies_count;
    create_info.pDependencies = dependencies;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;

    KRAFT_VK_CHECK(
        vkCreateRenderPass(context->LogicalDevice.Handle, &create_info, context->AllocationCallbacks, &out->Handle)
    );
    KRAFT_RENDERER_SET_OBJECT_NAME(out->Handle, VK_OBJECT_TYPE_RENDER_PASS, debug_name);
}

void VulkanBeginRenderPass(
    VulkanCommandBuffer* cmd_buf,
    VulkanRenderPass*    pass,
    VkFramebuffer        framebuffer,
    VkSubpassContents    contents
)
{
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.framebuffer = framebuffer;
    info.renderPass = pass->Handle;
    info.renderArea.offset.x = (i32)pass->Rect.x;
    info.renderArea.offset.y = (i32)pass->Rect.y;
    info.renderArea.extent.width = (u32)pass->Rect.z;
    info.renderArea.extent.height = (u32)pass->Rect.w;

    VkClearValue clear_values[2];
    clear_values[0].color.float32[0] = pass->Color.r;
    clear_values[0].color.float32[1] = pass->Color.g;
    clear_values[0].color.float32[2] = pass->Color.b;
    clear_values[0].color.float32[3] = pass->Color.a;
    clear_values[1].depthStencil.depth = pass->Depth;
    clear_values[1].depthStencil.stencil = pass->Stencil;

    info.clearValueCount = 2;
    info.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd_buf->Resource, &info, contents);
    cmd_buf->State = VULKAN_COMMAND_BUFFER_STATE_IN_RENDER_PASS;
}

void VulkanEndRenderPass(VulkanCommandBuffer* cmd_buf, VulkanRenderPass* pass)
{
    vkCmdEndRenderPass(cmd_buf->Resource);
    cmd_buf->State = VULKAN_COMMAND_BUFFER_STATE_RECORDING;
}

void VulkanDestroyRenderPass(VulkanContext* context, VulkanRenderPass* pass)
{
    if (pass && pass->Handle)
    {
        vkDestroyRenderPass(context->LogicalDevice.Handle, pass->Handle, context->AllocationCallbacks);
        pass->Handle = 0;
    }
}

} // namespace kraft::r