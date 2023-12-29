#pragma once

#include <GLFW/glfw3.h>

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft
{

void VulkanImguiInit(VulkanContext* context);
void VulkanImguiPostAPIInit(VulkanContext* context);
void VulkanImguiBeginFrame(VulkanContext* context);
void VulkanImguiEndFrame(VulkanContext* context);
void VulkanImguiDestroy(VulkanContext* context);

// Only valid if multi-viewports is enabled
void VulkanImguiEndFrameUpdatePlatformWindows(VulkanContext* context);

}