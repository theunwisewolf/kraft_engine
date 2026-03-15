#pragma once

#include <core/kraft_core.h>

namespace kraft::r {

struct VulkanContext;
struct VulkanCommandBuffer;
struct VulkanRenderPass;

void VulkanCreateRenderPass(VulkanContext* context, Vec4f color, Vec4f rect, f32 depth, u32 stencil, VulkanRenderPass* out, bool SwapchainTarget, const char* DebugName);
void VulkanBeginRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass, VkFramebuffer frameBuffer, VkSubpassContents contents);

void VulkanEndRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass);
void VulkanDestroyRenderPass(VulkanContext* context, VulkanRenderPass* pass);

};