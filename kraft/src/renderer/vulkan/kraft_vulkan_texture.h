#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "resources/kraft_resource_types.h"

namespace kraft::renderer
{

void VulkanCreateTexture(VulkanContext* context, uint8* data, Texture* out);
void VulkanDestroyTexture(VulkanContext* context, Texture* texture);
void VulkanCreateDefaultTexture(VulkanContext* context, uint32 width, uint32 height, uint8 channels, Texture* out);

}