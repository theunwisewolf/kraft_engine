#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

bool VulkanCreateGraphicsPipeline(VulkanContext* context, VulkanRenderPass* pass, VulkanPipelineDescription desc, VulkanPipeline* out);
void VulkanDestroyPipeline(VulkanContext* context, VulkanPipeline* pipeline);
void VulkanBindPipeline(VulkanCommandBuffer* buffer, VkPipelineBindPoint bindPoint, VulkanPipeline* pipeline);

}