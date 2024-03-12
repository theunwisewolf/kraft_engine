#include "kraft_renderer_imgui.h"

#include "core/kraft_log.h"
#include "core/kraft_application.h"
#include "renderer/vulkan/kraft_vulkan_imgui.h"

#include <imgui/imgui.h>

namespace kraft::renderer
{

static bool WidgetsNeedRefresh = false;

bool RendererCreateImguiBackend(RendererBackendType type, RendererImguiBackend* backend)
{
    if (type == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        backend->Init       = VulkanImgui::Init;
        backend->Destroy    = VulkanImgui::Destroy;
        backend->BeginFrame = VulkanImgui::BeginFrame;
        backend->EndFrame   = VulkanImgui::EndFrame;
        backend->AddTexture = VulkanImgui::AddTexture;
        
        return true;
    }
    else if (type == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {
    }

    return false;
}

bool RendererImGui::Init(ApplicationConfig* config)
{
    BackendMemory = MallocBlock(sizeof(RendererImguiBackend));
    Backend = (RendererImguiBackend*)BackendMemory.Data;

    if (!RendererCreateImguiBackend(config->RendererBackend, Backend))
    {
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    IniFilename = String(256, 0);
    StringFormat(*IniFilename, IniFilename.Allocated, "%s/kraft_imgui_config.ini", kraft::Application::Get()->BasePath.Data());
    io.IniFilename = *IniFilename;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    Backend->Init();

    return true;
}

void RendererImGui::AddWidget(const String& Name, ImGuiRenderCallback Callback)
{
    ImGuiWidget Widget(Name, Callback);
    Widgets.Push(Widget);

    KINFO("Added imgui widget %s", *Name);
}

void* RendererImGui::AddTexture(Texture* Texture)
{
    return Backend->AddTexture(Texture);
}

void RendererImGui::RenderWidgets()
{
    uint64 Size = Widgets.Length;
    for (int i = 0; i < Size; i++)
    {
        Widgets[i].Callback(WidgetsNeedRefresh);
    }

    WidgetsNeedRefresh = false;
}

void RendererImGui::OnResize(int width, int height)
{
    WidgetsNeedRefresh = true;
}

void RendererImGui::Destroy()
{
    Backend->Destroy();
}

// Only valid if multi-viewports is enabled
void RendererImGui::EndFrameUpdatePlatformWindows()
{
    // Update and Render additional Platform Windows
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

}