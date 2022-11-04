#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

void InitTestScene(VulkanContext* context);
void RenderTestScene(VulkanContext* context, VulkanCommandBuffer* buffer);
void DestroyTestScene(VulkanContext* context);

}