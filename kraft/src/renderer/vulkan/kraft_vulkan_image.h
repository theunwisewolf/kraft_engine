#pragma once

namespace kraft::r {

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