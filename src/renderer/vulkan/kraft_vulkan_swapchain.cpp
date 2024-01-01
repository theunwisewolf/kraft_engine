#include "kraft_vulkan_swapchain.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "renderer/vulkan/kraft_vulkan_device.h"
#include "renderer/vulkan/kraft_vulkan_image.h"

namespace kraft
{

#define KRAFT_CLAMP(value, min, max) (value < min) ? value = min : (value > max ? max : value);

void VulkanCreateSwapchain(VulkanContext* context, uint32 width, uint32 height, VulkanSwapchain* out)
{
    // context->Swapchain = {};

    if (out)
        context->Swapchain = *out;

    VkExtent2D extent = {width, height};

    // Query swapchain support info once more to get the most updated values
    // in case of window resize
    VulkanGetSwapchainSupportInfo(context->PhysicalDevice.Handle, context->Surface, &context->PhysicalDevice.SwapchainSupportInfo);

    // Find a suitable image format
    VulkanSwapchainSupportInfo info = context->PhysicalDevice.SwapchainSupportInfo;
    // If only 1 format is present and that is VK_FORMAT_UNDEFINED, then this means 
    // that there is no preferred format & we can choose any format we want
    if (info.FormatCount == 1 && info.Formats[0].format == VK_FORMAT_UNDEFINED)
    {
        VkSurfaceFormatKHR format = { VK_FORMAT_B8G8R8A8_UNORM, info.Formats[0].colorSpace };
        context->Swapchain.ImageFormat = format;
    }
    else
    {
        bool found = false;
        for (uint32 i = 0; i < info.FormatCount; ++i)
        {
            VkSurfaceFormatKHR format = info.Formats[i];
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                context->Swapchain.ImageFormat = format;
                found = true;
                break;
            }
        }

        // Choose whatever is available
        if (!found)
        {
            context->Swapchain.ImageFormat = info.Formats[0];
        }
    }

    // Present mode
    // Choose FIFO as default
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32 i = 0; i < info.PresentModeCount; ++i)
    {
        // If Mailbox presentation mode is supported, we want that
        if (info.PresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#_surface_capabilities
    if (info.SurfaceCapabilities.currentExtent.width != 0xFFFFFFFF)
    {
        extent = info.SurfaceCapabilities.currentExtent;
    }

    // Ensure our extents are allowed by the GPU
    VkExtent2D minExtent = info.SurfaceCapabilities.minImageExtent;
    VkExtent2D maxExtent = info.SurfaceCapabilities.maxImageExtent;
    extent.width = KRAFT_CLAMP(extent.width, minExtent.width, maxExtent.width);
    extent.height = KRAFT_CLAMP(extent.height, minExtent.height, maxExtent.height);

    // Write back the updated framebuffer width and height
    context->FramebufferWidth = extent.width;
    context->FramebufferHeight = extent.height;

    context->Swapchain.ImageCount = info.SurfaceCapabilities.minImageCount + 1;
    if (info.SurfaceCapabilities.maxImageCount > 0 && context->Swapchain.ImageCount > info.SurfaceCapabilities.maxImageCount)
    {
        context->Swapchain.ImageCount = info.SurfaceCapabilities.maxImageCount;
    }

    context->Swapchain.MaxFramesInFlight = context->Swapchain.ImageCount - 1;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = context->Surface;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageFormat = context->Swapchain.ImageFormat.format;
    swapchainCreateInfo.imageColorSpace = context->Swapchain.ImageFormat.colorSpace;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.minImageCount = context->Swapchain.ImageCount;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = info.SurfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    const uint32 queueFamilyIndices[2] = {
        context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex,
        context->PhysicalDevice.QueueFamilyInfo.PresentQueueIndex
    };

    if (context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex != context->PhysicalDevice.QueueFamilyInfo.PresentQueueIndex)
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    KRAFT_VK_CHECK(vkCreateSwapchainKHR(context->LogicalDevice.Handle, &swapchainCreateInfo, context->AllocationCallbacks, &context->Swapchain.Handle));

    context->Swapchain.ImageCount = 0;
    KRAFT_VK_CHECK(vkGetSwapchainImagesKHR(context->LogicalDevice.Handle, context->Swapchain.Handle, &context->Swapchain.ImageCount, 0));
    if (!context->Swapchain.Images)
    {
        context->Swapchain.Images = (VkImage*)kraft::Malloc(sizeof(VkImage) * context->Swapchain.ImageCount, MEMORY_TAG_RENDERER);
    }

    if (!context->Swapchain.ImageViews)
    {
        context->Swapchain.ImageViews = (VkImageView*)kraft::Malloc(sizeof(VkImageView) * context->Swapchain.ImageCount, MEMORY_TAG_RENDERER);
    }

    // Grab the images from the swapchain
    KRAFT_VK_CHECK(vkGetSwapchainImagesKHR(context->LogicalDevice.Handle, context->Swapchain.Handle, &context->Swapchain.ImageCount, context->Swapchain.Images));

    // Create Image views
    for (uint32 i = 0; i < context->Swapchain.ImageCount; ++i)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image    = context->Swapchain.Images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format   = context->Swapchain.ImageFormat.format;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        KRAFT_VK_CHECK(vkCreateImageView(context->LogicalDevice.Handle, &createInfo, context->AllocationCallbacks, &context->Swapchain.ImageViews[i]));
    }

    // Depth buffer if required
    if (context->PhysicalDevice.DepthBufferFormat != VK_FORMAT_UNDEFINED)
    {
        VulkanImageDescription desc;
        desc.Type        = VK_IMAGE_TYPE_2D;
        desc.Width       = extent.width;
        desc.Height      = extent.height;
        desc.Format      = context->PhysicalDevice.DepthBufferFormat;
        desc.Tiling      = VK_IMAGE_TILING_OPTIMAL;
        desc.UsageFlags  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        desc.AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

        VulkanCreateImage(context, desc, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, &context->Swapchain.DepthAttachment);
    }

    KSUCCESS(TEXT("[VulkanCreateSwapchain]: Swapchain created successfully!"));
}

void VulkanDestroySwapchain(VulkanContext* context)
{
    vkDeviceWaitIdle(context->LogicalDevice.Handle);
    for (uint32 i = 0; i < context->Swapchain.ImageCount; ++i)
    {
        vkDestroyImageView(context->LogicalDevice.Handle, context->Swapchain.ImageViews[i], context->AllocationCallbacks);
        context->Swapchain.ImageViews[i] = 0;
    }

    VulkanDestroyImage(context, &context->Swapchain.DepthAttachment);
    vkDestroySwapchainKHR(context->LogicalDevice.Handle, context->Swapchain.Handle, context->AllocationCallbacks);
}

void VulkanRecreateSwapchain(VulkanContext* context)
{
    VulkanDestroySwapchain(context);
    VulkanCreateSwapchain(context, context->FramebufferWidth, context->FramebufferHeight);
}

bool VulkanAcquireNextImageIndex(VulkanContext* context, uint64 timeoutNS, VkSemaphore imageAvailableSemaphore, VkFence fence, uint32* out)
{
    VkResult result = vkAcquireNextImageKHR(context->LogicalDevice.Handle, context->Swapchain.Handle, timeoutNS, imageAvailableSemaphore, fence, out);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        VulkanRecreateSwapchain(context);
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        KERROR(TEXT("[VulkanAcquireNextImageIndex]: Failed to acquire swapchain image"));
        return false;
    }

    return true;
}

void VulkanPresentSwapchain(VulkanContext* context, VkQueue presentQueue, VkSemaphore renderCompleteSemaphore, uint32 presentImageIndex)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderCompleteSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &context->Swapchain.Handle;
    presentInfo.pImageIndices = &presentImageIndex;
    presentInfo.pResults = 0;

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
    // Common case
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
    {
        context->Swapchain.CurrentFrame = (context->Swapchain.CurrentFrame + 1) % context->Swapchain.MaxFramesInFlight;
    }
    else if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        VulkanRecreateSwapchain(context);
    }
    else
    {
        KERROR(TEXT("[VulkanPresentSwapchain]: Presentation failed!"));
    }
}

}