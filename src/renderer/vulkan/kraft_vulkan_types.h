#pragma once

#include "core/kraft_core.h"
#include "core/kraft_asserts.h"

#include <vulkan/vulkan.h>

#define KRAFT_VK_CHECK(expression) \
    KRAFT_ASSERT(expression == VK_SUCCESS)

namespace kraft
{

struct VulkanContext
{
    VkInstance              Instance;
    VkAllocationCallbacks*  AllocationCallbacks;

#ifdef KRAFT_DEBUG
    VkDebugUtilsMessengerEXT DebugMessenger;
#endif
};

}