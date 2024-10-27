#include "kraft_renderer_imgui.h"

#include "core/kraft_application.h"
#include "core/kraft_log.h"
#include "renderer/vulkan/kraft_vulkan_imgui.h"

#include <imgui/extensions/imguizmo/ImGuizmo.h>
#include <imgui/imgui.h>

namespace kraft::renderer {

static bool                  WidgetsNeedRefresh = false;
static RendererImguiBackend* Backend = nullptr;

bool RendererCreateImguiBackend(RendererBackendType type, RendererImguiBackend* backend)
{
    if (type == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        backend->Init = VulkanImgui::Init;
        backend->Destroy = VulkanImgui::Destroy;
        backend->BeginFrame = VulkanImgui::BeginFrame;
        backend->EndFrame = VulkanImgui::EndFrame;
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

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

    IniFilename = String(256, 0);
    StringFormat(*IniFilename, (int)IniFilename.Allocated, "%s/kraft_imgui_config.ini", kraft::Application::Get()->BasePath.Data());
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
    // ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    static ImGuiDockNodeFlags dockspace_flags = 0;

    ImGuiWindowFlags     window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    static bool p_open = true;
    ImGui::Begin("DockSpace Demo", &p_open, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    // const float titlebarHeight = 57.0f;
    // const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

    // ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y));
    // const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
    // const ImVec2 titlebarMax = { ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f,
    //                                 ImGui::GetCursorScreenPos().y + titlebarHeight };
    // auto* drawList = ImGui::GetWindowDrawList();
    // drawList->AddRectFilled(titlebarMin, titlebarMax, Colours::Theme::titlebar);

    // // Logo
    // {
    //     const int logoWidth = EditorResources::HazelLogoTexture->GetWidth();
    //     const int logoHeight = EditorResources::HazelLogoTexture->GetHeight();
    //     const ImVec2 logoOffset(16.0f + windowPadding.x, 8.0f + windowPadding.y);
    //     const ImVec2 logoRectStart = { ImGui::GetItemRectMin().x + logoOffset.x, ImGui::GetItemRectMin().y + logoOffset.y };
    //     const ImVec2 logoRectMax = { logoRectStart.x + logoWidth, logoRectStart.y + logoHeight };
    //     drawList->AddImage(UI::GetTextureID(EditorResources::HazelLogoTexture), logoRectStart, logoRectMax);
    // }

    // ImGui::BeginHorizontal("Titlebar", { ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetFrameHeightWithSpacing() });

    // static float moveOffsetX;
    // static float moveOffsetY;
    // const float w = ImGui::GetContentRegionAvail().x;
    // const float buttonsAreaWidth = 94;

    // ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w - buttonsAreaWidth, titlebarHeight));

    ImGui::DockSpace(DockspaceID, ImVec2(0.0f, 0.0f), dockspace_flags);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Separator();

            if (ImGui::MenuItem("Flag: NoDockingOverCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingOverCentralNode) != 0))
            {
                dockspace_flags ^= ImGuiDockNodeFlags_NoDockingOverCentralNode;
            }
            if (ImGui::MenuItem("Flag: NoDockingSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingSplit) != 0))
            {
                dockspace_flags ^= ImGuiDockNodeFlags_NoDockingSplit;
            }
            if (ImGui::MenuItem("Flag: NoUndocking", "", (dockspace_flags & ImGuiDockNodeFlags_NoUndocking) != 0))
            {
                dockspace_flags ^= ImGuiDockNodeFlags_NoUndocking;
            }
            if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0))
            {
                dockspace_flags ^= ImGuiDockNodeFlags_NoResize;
            }
            if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0))
            {
                dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar;
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
                p_open = false;
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();

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