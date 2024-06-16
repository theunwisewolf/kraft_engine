#include "kraft_renderer_imgui.h"

#include "core/kraft_log.h"
#include "core/kraft_application.h"
#include "renderer/vulkan/kraft_vulkan_imgui.h"

#include <imgui/imgui.h>
#include <imgui/extensions/imguizmo/ImGuizmo.h>

namespace kraft::renderer
{

static bool WidgetsNeedRefresh = false;
static RendererImguiBackend* Backend = nullptr;

bool RendererCreateImguiBackend(RendererBackendType type, RendererImguiBackend* backend)
{
    if (type == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        backend->Init       = VulkanImgui::Init;
        backend->Destroy    = VulkanImgui::Destroy;
        backend->BeginFrame = VulkanImgui::BeginFrame;
        backend->EndFrame   = VulkanImgui::EndFrame;
        backend->AddTexture = VulkanImgui::AddTexture;
        backend->RemoveTexture = VulkanImgui::RemoveTexture;
        backend->PostFrameCleanup = VulkanImgui::PostFrameCleanup;
        
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

ImTextureID RendererImGui::AddTexture(Handle<Texture> Resource)
{
    return Backend->AddTexture(Resource);
}

void RendererImGui::RemoveTexture(ImTextureID TextureID)
{
    return Backend->RemoveTexture(TextureID);
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

    Backend->PostFrameCleanup();
}

void RendererImGui::BeginFrame(float64 DeltaTime)
{
    Backend->BeginFrame(DeltaTime);
    ImGui::NewFrame();

    ImGuiID DockspaceID = ImGui::GetID("MainDockspace");
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // Extensions
    ImGuizmo::BeginFrame();
}

void RendererImGui::EndFrame()
{
    ImGui::Render();
    ImDrawData* DrawData = ImGui::GetDrawData();

    Backend->EndFrame(DrawData);
}

}