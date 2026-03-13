#include "kraft_vulkan_imgui.h"
#include "vulkan/vulkan_core.h"

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
#include <renderer/kraft_resource_pool.inl>
#include <renderer/vulkan/kraft_vulkan_backend.h>
#include <renderer/vulkan/kraft_vulkan_command_buffer.h>
#include <renderer/vulkan/kraft_vulkan_renderpass.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>

#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::r {

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
    Array<ImTextureID> textures_to_remove;
} State;

bool Init()
{
    VulkanContext*       context = VulkanRendererBackend::Context();
    VkDescriptorPoolSize pool_sizes[] = {
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

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    KRAFT_VK_CHECK(vkCreateDescriptorPool(context->LogicalDevice.Handle, &pool_info, context->AllocationCallbacks, &ImGuiDescriptorPool));

    Window*     kraft_window = Platform::GetWindow();
    GLFWwindow* platform_window = kraft_window->PlatformWindowHandle;

    KASSERT(platform_window);

    ImGui_ImplGlfw_InitForVulkan(platform_window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context->Instance;
    init_info.PhysicalDevice = context->PhysicalDevice.Handle;
    init_info.Device = context->LogicalDevice.Handle;
    init_info.QueueFamily = context->PhysicalDevice.QueueFamilyInfo.GraphicsQueueIndex;
    init_info.Queue = context->LogicalDevice.GraphicsQueue;
    init_info.PipelineCache = 0; // TODO: (amn) Pipeline cache
    init_info.DescriptorPool = ImGuiDescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = context->Swapchain.ImageCount;
    init_info.ImageCount = context->Swapchain.ImageCount;
    init_info.MSAASamples = context->Swapchain.MSAASampleCount;
    init_info.Allocator = context->AllocationCallbacks;
    init_info.CheckVkResultFn = CheckVkResult;

#if KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    init_info.UseDynamicRendering = true;
    init_info.RenderPass = nullptr;

    static const u64 color_format_count = 1;
    const VkFormat   color_formats[color_format_count] = {
        VK_FORMAT_R8G8B8A8_UNORM,
        // VK_FORMAT_A2B10G10R10_UNORM_PACK32, // HDR later
    };

    VkPipelineRenderingCreateInfo rendering_create_info = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rendering_create_info.colorAttachmentCount = color_format_count;
    rendering_create_info.pColorAttachmentFormats = color_formats;
    rendering_create_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT; // TODO: Read from config
    init_info.PipelineRenderingCreateInfo = rendering_create_info;
#else
    initInfo.RenderPass = context->MainRenderPass.Handle;
#endif

    ImGui_ImplVulkan_Init(&init_info);

    return true;
}

bool BeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    return true;
}

bool EndFrame(ImDrawData* draw_data)
{
    VulkanContext*       context = VulkanRendererBackend::Context();
    VulkanCommandBuffer* parent_command_buffer = VulkanResourceManagerApi::GetCommandBuffer(context->ActiveCommandBuffer);

    ImGui_ImplVulkan_RenderDrawData(draw_data, parent_command_buffer->Resource);

    return true;
}

ImTextureID AddTexture(Handle<Texture> texture_handle, Handle<TextureSampler> sampler_handle)
{
    VulkanTexture*        texture = VulkanResourceManagerApi::GetTexture(texture_handle);
    VulkanTextureSampler* sampler = VulkanResourceManagerApi::GetTextureSampler(sampler_handle);

    return ImGui_ImplVulkan_AddTexture(sampler->Sampler, texture->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void RemoveTexture(ImTextureID texture_id)
{
    State.textures_to_remove.Push(texture_id);
}

void PostFrameCleanup()
{
    for (i32 i = 0; i < State.textures_to_remove.Size(); i++)
    {
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)State.textures_to_remove[i]);
    }

    State.textures_to_remove.Clear();
}

bool Destroy()
{
    VulkanContext* context = VulkanRendererBackend::Context();
    vkDeviceWaitIdle(context->LogicalDevice.Handle);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(context->LogicalDevice.Handle, ImGuiDescriptorPool, context->AllocationCallbacks);

    return true;
}

} // namespace VulkanImgui
} // namespace kraft::r