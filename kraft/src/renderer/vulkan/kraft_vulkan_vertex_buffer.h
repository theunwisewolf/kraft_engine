#pragma once

#include "renderer/vulkan/kraft_vulkan_types.h"

namespace kraft::renderer
{

bool VulkanCreateVertexBuffer(VulkanContext* context, const void* data, uint64 size, VulkanBuffer* out);

}