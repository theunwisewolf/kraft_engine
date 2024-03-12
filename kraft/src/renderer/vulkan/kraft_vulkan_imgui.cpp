#include "kraft_vulkan_imgui.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_asserts.h"
#include "core/kraft_application.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_window.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/vulkan/kraft_vulkan_backend.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"
#include "renderer/vulkan/kraft_vulkan_renderpass.h"

namespace kraft::renderer
{

namespace VulkanImgui
{

// TODO: (amn) All of this code needs rework
static VulkanCommandBuffer CommandBuffers[3];
static VkDescriptorPool ImGuiDescriptorPool;

// TODO (Amn): Improve this?
static void CheckVkResult(VkResult err)
{
    if (err == 0)
        return;

    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

bool Init()
{
    VulkanContext *context = VulkanRendererBackend::GetContext();
    VkDescriptorPoolSize poolSizes[] =
    {
        // { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        // { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        // { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        // { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        // { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        // { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        // { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        // { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        // { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        // { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    KRAFT_VK_CHECK(vkCreateDescriptorPool(context->LogicalDevice.Handle, &poolInfo, context->AllocationCallbacks, &ImGuiDescriptorPool));

    Window& KraftWindow = Platform::GetWindow();
    GLFWwindow* window = KraftWindow.PlatformWindowHandle;

    KASSERT(window);

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = context->Instance;
    initInfo.PhysicalDevice = context->PhysicalDevice.Handle;
    initInfo.Device = context->LogicalDevice.Handle;
    initInfo.QueueFamily = context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex;
    initInfo.Queue = context->LogicalDevice.GraphicsQueue;
    initInfo.PipelineCache = 0; // TODO: (amn) Pipeline cache
    initInfo.DescriptorPool = ImGuiDescriptorPool;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = context->Swapchain.ImageCount;
    initInfo.ImageCount = context->Swapchain.ImageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = context->AllocationCallbacks;
    initInfo.CheckVkResultFn = CheckVkResult;

    ImGui_ImplVulkan_Init(&initInfo, context->MainRenderPass.Handle);

    // Upload Fonts
    {
        // VulkanCommandBuffer buffer;
        // VulkanAllocateAndBeginSingleUseCommandBuffer(context, context->GraphicsCommandPool, &buffer);
        // ImGui_ImplVulkan_CreateFontsTexture(buffer.Handle);
        // VulkanEndAndSubmitSingleUseCommandBuffer(context, context->GraphicsCommandPool, &buffer, context->LogicalDevice.GraphicsQueue);

        // KRAFT_VK_CHECK(vkDeviceWaitIdle(context->LogicalDevice.Handle));
        // ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    // Command Buffers
    for (uint32 i = 0; i < context->Swapchain.ImageCount; ++i)
    {
        // VulkanAllocateCommandBuffer(context, context->GraphicsCommandPool, false, &CommandBuffers[i]);
    }

    return true;
}

bool BeginFrame(float64 deltaTime)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return true;
}

bool EndFrame(float64 deltaTime)
{
    VulkanContext *context = VulkanRendererBackend::GetContext();
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    uint32 width = context->FramebufferWidth;
    uint32 height = context->FramebufferHeight;

    VulkanCommandBuffer* parentCommandBuffer = &context->GraphicsCommandBuffers[context->CurrentSwapchainImageIndex];
    // VulkanBeginCommandBuffer(parentCommandBuffer, true, false, false);
    // VulkanBeginRenderPass(parentCommandBuffer, &context->MainRenderPass, context->Swapchain.Framebuffers[context->CurrentSwapchainImageIndex].Handle, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    // VkClearValue clearValues[2];
    // clearValues[0].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
    // clearValues[1].depthStencil = { 1.0f, 0 };

    // VkRenderPassBeginInfo renderPassBeginInfo = {};
    // renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    // renderPassBeginInfo.pNext = nullptr;
    // renderPassBeginInfo.renderPass = context->MainRenderPass.Handle;
    // renderPassBeginInfo.renderArea.offset.x = 0;
    // renderPassBeginInfo.renderArea.offset.y = 0;
    // renderPassBeginInfo.renderArea.extent.width = width;
    // renderPassBeginInfo.renderArea.extent.height = height;
    // renderPassBeginInfo.clearValueCount = 2; // Color + depth
    // renderPassBeginInfo.pClearValues = clearValues;
    // renderPassBeginInfo.framebuffer = context->Swapchain.Framebuffers[context->CurrentSwapchainImageIndex].Handle;

    // vkCmdBeginRenderPass(parentCommandBuffer->Handle, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    {
        // VulkanCommandBuffer* commandBuffer = &CommandBuffers[context->CurrentSwapchainImageIndex];
        // VkCommandBufferInheritanceInfo inheritanceInfo = {};
        // inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        // inheritanceInfo.renderPass = context->MainRenderPass.Handle;
        // inheritanceInfo.framebuffer = context->Swapchain.Framebuffers[context->CurrentSwapchainImageIndex].Handle;

        // VkCommandBufferBeginInfo bufferBeginInfo = {};
        // bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        // bufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

        // KRAFT_VK_CHECK(vkBeginCommandBuffer(commandBuffer->Handle, &bufferBeginInfo));

        // VkViewport viewport = {};
		// viewport.x = 0.0f;
		// viewport.y = (float)height;
		// viewport.width = (float)width;
		// viewport.height = -(float)height;
		// viewport.minDepth = 0.0f;
		// viewport.maxDepth = 1.0f;
		// vkCmdSetViewport(commandBuffer->Handle, 0, 1, &viewport);

		// VkRect2D scissor = {};
		// scissor.extent.width = width;
		// scissor.extent.height = height;
		// vkCmdSetScissor(commandBuffer->Handle, 0, 1, &scissor);

        // Record dear imgui primitives into command buffer
        ImGui_ImplVulkan_RenderDrawData(drawData, parentCommandBuffer->Handle);

        // KRAFT_VK_CHECK(vkEndCommandBuffer(commandBuffer->Handle));

        // vkCmdExecuteCommands(parentCommandBuffer->Handle, 1, &commandBuffer->Handle);
    }

    // VulkanEndRenderPass(parentCommandBuffer, &context->MainRenderPass);
    // VulkanEndCommandBuffer(parentCommandBuffer);

    return true;
}

void* AddTexture(Texture* Texture)
{
    VulkanTexture* BackendTexture = (VulkanTexture*)Texture->RendererData;
    return ImGui_ImplVulkan_AddTexture(BackendTexture->Sampler, BackendTexture->Image.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); 
}

bool Destroy()
{
    VulkanContext *context = VulkanRendererBackend::GetContext();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(context->LogicalDevice.Handle, ImGuiDescriptorPool, context->AllocationCallbacks);

    return true;
}

}
}