#include "kraft_vulkan_imgui.h"

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/imgui.h>

#include <imgui/backends/imgui_impl_vulkan.h>
#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include "core/kraft_application.h"
#include "core/kraft_asserts.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_window.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/vulkan/kraft_vulkan_backend.h"
#include "renderer/vulkan/kraft_vulkan_command_buffer.h"
#include "renderer/vulkan/kraft_vulkan_renderpass.h"

#include <containers/kraft_array.h>
#include <platform/kraft_filesystem.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>

namespace kraft::renderer {

namespace VulkanImgui {

// TODO: (amn) All of this code needs rework
static VulkanCommandBuffer CommandBuffers[3];
static VkDescriptorPool    ImGuiDescriptorPool;

// TODO (Amn): Improve this?
static void CheckVkResult(VkResult err)
{
    if (err == 0)
        return;

    fprintf(stderr, "[vulkan] Error: VkResult = %s\n", string_VkResult(err));
    if (err < 0)
        abort();
}

struct ImguiBackendState
{
    Array<ImTextureID> TexturesToRemove;
} State;

bool Init()
{
    VulkanContext*       context = VulkanRendererBackend::Context();
    VkDescriptorPoolSize poolSizes[] = {
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

    Window&     KraftWindow = Platform::GetWindow();
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
    initInfo.RenderPass = context->MainRenderPass.Handle;

    ImGui_ImplVulkan_Init(&initInfo);

    // Upload Fonts
    ImGuiIO& IO = ImGui::GetIO();
    {
        uint8* Buffer = nullptr;
        uint64 Size = 0;
        bool   JakartaRegular = kraft::filesystem::ReadAllBytes("res/fonts/PlusJakartaSans-Regular.ttf", &Buffer, &Size);
        KASSERT(JakartaRegular);
        IO.Fonts->AddFontFromMemoryTTF(Buffer, (int)Size, 14);
        // VulkanCommandBuffer buffer;
        // VulkanAllocateAndBeginSingleUseCommandBuffer(context, context->GraphicsCommandPool, &buffer);
        // ImGui_ImplVulkan_CreateFontsTexture();
        // VulkanEndAndSubmitSingleUseCommandBuffer(context, context->GraphicsCommandPool, &buffer, context->LogicalDevice.GraphicsQueue);

        // KRAFT_VK_CHECK(vkDeviceWaitIdle(context->LogicalDevice.Handle));
        // ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    ImGui::GetStyle().WindowPadding = ImVec2(14, 8);
    ImGui::GetStyle().FramePadding = ImVec2(10, 8);
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 12);
    ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 4);

    // Borders
    ImGui::GetStyle().WindowBorderSize = 1.0f;
    ImGui::GetStyle().ChildBorderSize = 1.0f;
    ImGui::GetStyle().PopupBorderSize = 1.0f;
    ImGui::GetStyle().FrameBorderSize = 1.0f;

    // Border Rounding
    ImGui::GetStyle().FrameRounding = 2.0f;

    // Colors
    {
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.90f, 0.28f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.93f, 0.36f, 0.37f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.93f, 0.36f, 0.37f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(1.00f, 0.09f, 0.25f, 0.18f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(1.00f, 0.09f, 0.25f, 0.18f);
        colors[ImGuiCol_TabHovered] = ImVec4(1.00f, 0.04f, 0.23f, 0.27f);
        colors[ImGuiCol_TabActive] = ImVec4(1.00f, 0.04f, 0.23f, 0.27f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(1.00f, 0.09f, 0.25f, 0.18f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
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

    return true;
}

bool EndFrame(ImDrawData* DrawData)
{
    VulkanContext*       Context = VulkanRendererBackend::Context();
    VulkanCommandBuffer* parentCommandBuffer = &Context->GraphicsCommandBuffers[Context->CurrentSwapchainImageIndex];
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
        ImGui_ImplVulkan_RenderDrawData(DrawData, parentCommandBuffer->Resource);

        // KRAFT_VK_CHECK(vkEndCommandBuffer(commandBuffer->Handle));

        // vkCmdExecuteCommands(parentCommandBuffer->Handle, 1, &commandBuffer->Handle);
    }

    // VulkanEndRenderPass(parentCommandBuffer, &context->MainRenderPass);
    // VulkanEndCommandBuffer(parentCommandBuffer);

    return true;
}

ImTextureID AddTexture(Handle<Texture> Resource)
{
    VulkanContext* Context = VulkanRendererBackend::Context();
    const auto&    TexturePool = Context->ResourceManager->GetTexturePool();
    VulkanTexture* BackendTexture = TexturePool.Get(Resource);

    return ImGui_ImplVulkan_AddTexture(BackendTexture->Sampler, BackendTexture->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void RemoveTexture(ImTextureID TextureID)
{
    State.TexturesToRemove.Push(TextureID);
}

void PostFrameCleanup()
{
    for (int i = 0; i < State.TexturesToRemove.Size(); i++)
    {
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)State.TexturesToRemove[i]);
    }

    State.TexturesToRemove.Clear();
}

bool Destroy()
{
    VulkanContext* context = VulkanRendererBackend::Context();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(context->LogicalDevice.Handle, ImGuiDescriptorPool, context->AllocationCallbacks);

    return true;
}

}
}