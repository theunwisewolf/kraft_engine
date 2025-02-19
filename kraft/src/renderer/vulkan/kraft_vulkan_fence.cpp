#include <volk/volk.h>

#include "kraft_vulkan_fence.h"

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>

#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::renderer {

void VulkanCreateFence(VulkanContext* context, bool signalled, VulkanFence* out)
{
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    out->Signalled = signalled;
    KRAFT_VK_CHECK(vkCreateFence(context->LogicalDevice.Handle, &info, context->AllocationCallbacks, &out->Handle));
}

void VulkanDestroyFence(VulkanContext* context, VulkanFence* fence)
{
    if (fence->Handle)
    {
        vkDestroyFence(context->LogicalDevice.Handle, fence->Handle, context->AllocationCallbacks);
    }

    fence->Handle = 0;
    fence->Signalled = false;
}

void VulkanResetFence(VulkanContext* context, VulkanFence* fence)
{
    KRAFT_VK_CHECK(vkResetFences(context->LogicalDevice.Handle, 1, &fence->Handle));
    fence->Signalled = false;
}

bool VulkanWaitForFence(VulkanContext* context, VulkanFence* fence, uint64 timeoutNS)
{
    if (fence->Signalled)
    {
        return true;
    }

    VkResult result = vkWaitForFences(context->LogicalDevice.Handle, 1, &fence->Handle, VK_TRUE, timeoutNS);
    switch (result)
    {
        case VK_SUCCESS:
        {
            fence->Signalled = true;
            return true;
        }
        break;
        case VK_TIMEOUT:
        {
            KWARN("[VulkanFenceWait]: VK_TIMEOUT");
        }
        break;
        case VK_ERROR_DEVICE_LOST:
        {
            KERROR("[VulkanFenceWait]: VK_ERROR_DEVICE_LOST");
        }
        break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        {
            KERROR("[VulkanFenceWait]: VK_ERROR_OUT_OF_HOST_MEMORY");
        }
        break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        {
            KERROR("[VulkanFenceWait]: VK_ERROR_OUT_OF_DEVICE_MEMORY");
        }
        break;
        default:
        {
            KERROR("[VulkanFenceWait]: Unknown error %d", result);
        }
    }

    return false;
}

}