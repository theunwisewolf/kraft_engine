#include <volk/volk.h>

#include "kraft_vulkan_swapchain.h"

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <renderer/vulkan/kraft_vulkan_device.h>
#include <renderer/vulkan/kraft_vulkan_image.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>

#include <kraft_types.h>
#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::r {

#define KRAFT_CLAMP(value, min, max) (value < min) ? value = min : (value > max ? max : value);

void VulkanCreateSwapchain(VulkanContext* context, u32 width, u32 height, bool enable_vsync, VulkanSwapchain* out)
{
    if (out)
        context->Swapchain = *out;

    VkExtent2D extent = { width, height };

    // Query swapchain support info once more to get the most updated values
    // in case of window resize
    KRAFT_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->PhysicalDevice.Handle, context->Surface, &context->PhysicalDevice.SwapchainSupportInfo.SurfaceCapabilities));
    // VulkanGetSwapchainSupportInfo(context->PhysicalDevice.Handle, context->Surface, &context->PhysicalDevice.SwapchainSupportInfo);

    // Find a suitable image format
    VulkanSwapchainSupportInfo info = context->PhysicalDevice.SwapchainSupportInfo;
    // If only 1 format is present and that is VK_FORMAT_UNDEFINED, then this means
    // that there is no preferred format & we can choose any format we want
    if (info.FormatCount == 1 && info.Formats[0].format == VK_FORMAT_UNDEFINED)
    {
        VkSurfaceFormatKHR format = { VK_FORMAT_R8G8B8A8_UNORM, info.Formats[0].colorSpace };
        context->Swapchain.ImageFormat = format;
    }
    else
    {
        bool found = false;
        for (u32 i = 0; i < info.FormatCount; ++i)
        {
            VkSurfaceFormatKHR surface_format = info.Formats[i];
            if ((surface_format.format == VK_FORMAT_R8G8B8A8_UNORM /*|| surface_format.format == VK_FORMAT_B8G8R8A8_UNORM*/) && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                context->Swapchain.ImageFormat = surface_format;
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
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkPresentModeKHR preferred_present_mode = enable_vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
    for (u32 i = 0; i < info.PresentModeCount; ++i)
    {
        // If Mailbox presentation mode is supported, we want that
        if (info.PresentModes[i] == preferred_present_mode)
        {
            present_mode = preferred_present_mode;
            break;
        }
    }

    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#_surface_capabilities
    if (info.SurfaceCapabilities.currentExtent.width != 0xFFFFFFFF)
    {
        extent = info.SurfaceCapabilities.currentExtent;
    }

    // Ensure our extents are allowed by the GPU
    VkExtent2D min_extent = info.SurfaceCapabilities.minImageExtent;
    VkExtent2D max_extent = info.SurfaceCapabilities.maxImageExtent;
    extent.width = KRAFT_CLAMP(extent.width, min_extent.width, max_extent.width);
    extent.height = KRAFT_CLAMP(extent.height, min_extent.height, max_extent.height);

    // Write back the updated framebuffer width and height
    context->FramebufferWidth = extent.width;
    context->FramebufferHeight = extent.height;

    context->Swapchain.ImageCount = info.SurfaceCapabilities.minImageCount + 1;
    if (info.SurfaceCapabilities.maxImageCount > 0 && context->Swapchain.ImageCount > info.SurfaceCapabilities.maxImageCount)
    {
        context->Swapchain.ImageCount = info.SurfaceCapabilities.maxImageCount;
    }

    context->Swapchain.MaxFramesInFlight = context->Swapchain.ImageCount - 1;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = context->Surface;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageFormat = context->Swapchain.ImageFormat.format;
    swapchain_create_info.imageColorSpace = context->Swapchain.ImageFormat.colorSpace;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.minImageCount = context->Swapchain.ImageCount;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.preTransform = info.SurfaceCapabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    const u32 queue_family_indices[2] = { (u32)context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex, (u32)context->PhysicalDevice.QueueFamilyInfo.PresentQueueIndex };

    if (context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex != context->PhysicalDevice.QueueFamilyInfo.PresentQueueIndex)
    {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    KRAFT_VK_CHECK(vkCreateSwapchainKHR(context->LogicalDevice.Handle, &swapchain_create_info, context->AllocationCallbacks, &context->Swapchain.Resource));

    context->Swapchain.ImageCount = 0;
    KRAFT_VK_CHECK(vkGetSwapchainImagesKHR(context->LogicalDevice.Handle, context->Swapchain.Resource, &context->Swapchain.ImageCount, 0));
    // TODO: Need a "renderer config" to actually set the required/max swapchain images
    context->Swapchain.ImageCount = (context->Swapchain.ImageCount > 3 ? 3 : context->Swapchain.ImageCount);

    // Grab the images from the swapchain
    KRAFT_VK_CHECK(vkGetSwapchainImagesKHR(context->LogicalDevice.Handle, context->Swapchain.Resource, &context->Swapchain.ImageCount, context->Swapchain.Images));

    // Create Image views
    for (u32 i = 0; i < context->Swapchain.ImageCount; ++i)
    {
        VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        info.image = context->Swapchain.Images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = context->Swapchain.ImageFormat.format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        KRAFT_VK_CHECK(vkCreateImageView(context->LogicalDevice.Handle, &info, context->AllocationCallbacks, &context->Swapchain.ImageViews[i]));
    }

    // Convert depth format
    Format::Enum depth_buffer_format = Format::Count;
    if (context->PhysicalDevice.DepthBufferFormat != VK_FORMAT_UNDEFINED)
    {
        switch (context->PhysicalDevice.DepthBufferFormat)
        {
            case VK_FORMAT_D16_UNORM:          depth_buffer_format = Format::D16_UNORM; break;
            case VK_FORMAT_D32_SFLOAT:         depth_buffer_format = Format::D32_SFLOAT; break;
            case VK_FORMAT_D16_UNORM_S8_UINT:  depth_buffer_format = Format::D16_UNORM_S8_UINT; break;
            case VK_FORMAT_D24_UNORM_S8_UINT:  depth_buffer_format = Format::D24_UNORM_S8_UINT; break;
            case VK_FORMAT_D32_SFLOAT_S8_UINT: depth_buffer_format = Format::D32_SFLOAT_S8_UINT; break;
            default:                           KFATAL("Unsupported depth buffer format %d", context->PhysicalDevice.DepthBufferFormat);
        }
    }

    TextureSampleCountFlags sample_count = TEXTURE_SAMPLE_COUNT_FLAGS_1;

    // MSAA
    // Figure out the sample count and create the required targets
    {
        u8                 requested = context->Options->MSAASamples;
        VkSampleCountFlags supported = context->PhysicalDevice.Properties.limits.framebufferColorSampleCounts & context->PhysicalDevice.Properties.limits.framebufferDepthSampleCounts;

        VkSampleCountFlagBits msaa = VK_SAMPLE_COUNT_1_BIT;
        if (requested >= 8 && (supported & VK_SAMPLE_COUNT_8_BIT))
            msaa = VK_SAMPLE_COUNT_8_BIT;
        else if (requested >= 4 && (supported & VK_SAMPLE_COUNT_4_BIT))
            msaa = VK_SAMPLE_COUNT_4_BIT;
        else if (requested >= 2 && (supported & VK_SAMPLE_COUNT_2_BIT))
            msaa = VK_SAMPLE_COUNT_2_BIT;

        context->Swapchain.MSAASampleCount = msaa;

        if (msaa != VK_SAMPLE_COUNT_1_BIT)
        {
            if (msaa == VK_SAMPLE_COUNT_2_BIT)
                sample_count = TEXTURE_SAMPLE_COUNT_FLAGS_2;
            else if (msaa == VK_SAMPLE_COUNT_4_BIT)
                sample_count = TEXTURE_SAMPLE_COUNT_FLAGS_4;
            else if (msaa == VK_SAMPLE_COUNT_8_BIT)
                sample_count = TEXTURE_SAMPLE_COUNT_FLAGS_8;

            context->Swapchain.MSAAColorAttachment = ResourceManager->CreateTexture({
                .DebugName = "MSAA-Color",
                .Dimensions = { (f32)extent.width, (f32)extent.height, 1, 4 },
                .Format = Format::RGBA8_UNORM,
                .Usage = (u64)(TextureUsageFlags::TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT | TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSIENT_ATTACHMENT),
                .SampleCount = sample_count,
            });

            KINFO("[VulkanCreateSwapchain]: MSAA enabled with %dx samples", (int)msaa);
        }
    }

    // Figure out the right depth format
    if (depth_buffer_format != Format::Count)
    {
        if (context->Swapchain.MSAASampleCount == VK_SAMPLE_COUNT_1_BIT)
        {
            context->Swapchain.DepthAttachment = ResourceManager->CreateTexture({
                .DebugName = "Swapchain-Depth",
                .Dimensions = { (f32)extent.width, (f32)extent.height, 1, 4 },
                .Format = depth_buffer_format,
                .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT,
            });
        }
        else
        {
            context->Swapchain.DepthAttachment = ResourceManager->CreateTexture({
                .DebugName = "MSAA-Depth",
                .Dimensions = { (f32)extent.width, (f32)extent.height, 1, 4 },
                .Format = depth_buffer_format,
                .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT,
                .SampleCount = sample_count,
            });
        }
    }

    KSUCCESS("[VulkanCreateSwapchain]: Swapchain created successfully!");
}

void VulkanDestroySwapchain(VulkanContext* context)
{
    vkDeviceWaitIdle(context->LogicalDevice.Handle);
    for (u32 i = 0; i < context->Swapchain.ImageCount; ++i)
    {
        vkDestroyImageView(context->LogicalDevice.Handle, context->Swapchain.ImageViews[i], context->AllocationCallbacks);
        context->Swapchain.ImageViews[i] = 0;
    }

    // ResourceManager->DestroyTexture(context->Swapchain.DepthAttachment);

    vkDestroySwapchainKHR(context->LogicalDevice.Handle, context->Swapchain.Resource, context->AllocationCallbacks);
}

void VulkanRecreateSwapchain(VulkanContext* context)
{
    VulkanDestroySwapchain(context);
    VulkanCreateSwapchain(context, context->FramebufferWidth, context->FramebufferHeight, context->Options->VSync);
}

bool VulkanAcquireNextImageIndex(VulkanContext* context, u64 timeout_nanoseconds, VkSemaphore image_available_semaphore, VkFence fence, u32* out)
{
    VkResult result = vkAcquireNextImageKHR(context->LogicalDevice.Handle, context->Swapchain.Resource, timeout_nanoseconds, image_available_semaphore, fence, out);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        VulkanRecreateSwapchain(context);
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        KERROR("[VulkanAcquireNextImageIndex]: Failed to acquire swapchain image");
        return false;
    }

    return true;
}

} // namespace kraft::r