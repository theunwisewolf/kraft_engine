#include "kraft_vulkan_imgui.h"

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>
#include <platform/kraft_filesystem.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <renderer/kraft_renderer_types.h>
#include <renderer/kraft_resource_pool.inl>
#include <renderer/vulkan/kraft_vulkan_backend.h>
#include <renderer/vulkan/kraft_vulkan_command_buffer.h>
#include <renderer/vulkan/kraft_vulkan_renderpass.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>

#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::renderer {

namespace VulkanImgui {

// TODO: (amn) All of this code needs rework
static VkDescriptorPool ImGuiDescriptorPool;

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

    Window*     KraftWindow = Platform::GetWindow();
    GLFWwindow* Window = KraftWindow->PlatformWindowHandle;

    KASSERT(Window);

    ImGui_ImplGlfw_InitForVulkan(Window, true);
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

    return true;
}

bool BeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    return true;
}

bool EndFrame(ImDrawData* DrawData)
{
    VulkanContext*       Context = VulkanRendererBackend::Context();
    VulkanCommandBuffer* ParentCommandBuffer = VulkanResourceManagerApi::GetCommandBuffer(Context->ActiveCommandBuffer);

    ImGui_ImplVulkan_RenderDrawData(DrawData, ParentCommandBuffer->Resource);

    return true;
}

ImTextureID AddTexture(Handle<Texture> TextureHandle, Handle<TextureSampler> TextureSamplerHandle)
{
    VulkanTexture*        BackendTexture = VulkanResourceManagerApi::GetTexture(TextureHandle);
    VulkanTextureSampler* Sampler = VulkanResourceManagerApi::GetTextureSampler(TextureSamplerHandle);

    return ImGui_ImplVulkan_AddTexture(Sampler->Sampler, BackendTexture->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
    VulkanContext* Context = VulkanRendererBackend::Context();
    vkDeviceWaitIdle(Context->LogicalDevice.Handle);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(Context->LogicalDevice.Handle, ImGuiDescriptorPool, Context->AllocationCallbacks);

    return true;
}

} // namespace VulkanImgui
} // namespace kraft::renderer