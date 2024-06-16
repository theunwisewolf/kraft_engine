#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

void VulkanTransitionImageLayout(
    VulkanContext* context,
    VulkanCommandBuffer commandBuffer,
    VkImage Image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout);

}