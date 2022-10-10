#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

void VulkanCreateRenderPass(VulkanContext* context, float4 color, float4 rect, float32 depth, uint32 stencil, VulkanRenderPass* out);
void VulkanBeginRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass, VkFramebuffer frameBuffer);
void VulkanEndRenderPass(VulkanCommandBuffer* commandBuffer, VulkanRenderPass* pass);
void VulkanDestroyRenderPass(VulkanContext* context, VulkanRenderPass* pass);

};