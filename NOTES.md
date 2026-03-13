## MSAA
To do multisampling, we must create a new color target with the correct sample count `VkImageCreateInfo.samples` and this is because we can't directly set the sample count for the swapchain image.
So we will be rendering to the MSAA color target and then "resolving" it to the swapchain color target. This works by setting the `resolve*` parameters of the `VkRenderingAttachmentInfo` struct:

```c++
if (msaa_enabled)
{
    // Render to MSAA image, resolve to swapchain
    color_attachment_info.imageView = VulkanResourceManagerApi::GetTexture(s_Context.Swapchain.MSAAColorAttachment)->View;
    color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    
    // MSAA image doesn't need to be stored
    color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_info.clearValue.color = { { 0x2c / 255.0f, 0x3e / 255.0f, 0x50 / 255.0f, 1.0f } };

    // Average here simply means "Average all samples per pixel to produce the final color"
    color_attachment_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    color_attachment_info.resolveImageView = s_Context.Swapchain.ImageViews[s_Context.CurrentSwapchainImageIndex];
    color_attachment_info.resolveImageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
}
```

For the depth-buffer, it is already created as a separate target for the swapchain, so we can just set the samples there.