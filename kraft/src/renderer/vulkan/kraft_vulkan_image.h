#pragma once

#include <core/kraft_core.h>
#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::renderer {

struct VulkanContext;
struct VulkanCommandBuffer;

void VulkanTransitionImageLayout(
    VulkanContext*       Context,
    VulkanCommandBuffer* GPUCmdBuffer,
    VkImage              Image,
    VkImageLayout        OldLayout,
    VkImageLayout        NewLayout
);

}