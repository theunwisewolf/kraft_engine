#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

void VulkanCreateRenderPass(VulkanContext* context, Vec4f color, Vec4f rect, float32 depth, uint32 stencil, VulkanRenderPass* out, bool SwapchainTarget, const char* DebugName);
void VulkanBeginRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass, VkFramebuffer frameBuffer, VkSubpassContents contents);
void VulkanBeginRenderPass(VulkanContext* Context, VulkanCommandBuffer* CmdBuffer, Handle<RenderPass> PassHandle, VkSubpassContents Contents);
void VulkanEndRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass);
void VulkanEndRenderPass(VulkanCommandBuffer* CmdBuffer);
void VulkanDestroyRenderPass(VulkanContext* context, VulkanRenderPass* pass);

};