#include <core/kraft_core.h>
#include <core/kraft_engine.h>
#include <core/kraft_time.h>
#include <kraft.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <renderer/kraft_renderer_frontend.h>

#include <kraft_types.h>

#include "imgui/imgui_renderer.h"

struct ApplicationState
{
    char          TitleBuffer[1024];
    float64       LastTime;
    float64       TimeSinceLastFrame;
    RendererImGui ImGuiRenderer;
    uint32        FrameCount;
} AppState;

int main(int argc, char** argv)
{
    bool ImGuiDemoWindowOpen = true;
    kraft::CreateEngine({
        .Argc = argc,
        .Argv = argv,
        .ApplicationName = "SampleApp",
        .ConsoleApp = false,
    });

    auto Window = kraft::CreateWindow("SampleApp", 1280, 768);
    auto Renderer = kraft::CreateRenderer({
        .Backend = kraft::RENDERER_BACKEND_TYPE_VULKAN,
    });

    Window->Maximize();

    AppState.ImGuiRenderer.Init();

    while (kraft::Engine::Running)
    {
        AppState.FrameCount++;

        float64 FrameStartTime = kraft::Platform::GetAbsoluteTime();
        float64 DeltaTime = FrameStartTime - AppState.LastTime;
        kraft::Time::DeltaTime = DeltaTime;
        AppState.TimeSinceLastFrame += DeltaTime;
        AppState.LastTime = FrameStartTime;

        // Poll events
        kraft::Engine::Tick();

        // We only want to render when the engine is not suspended
        if (!kraft::Engine::Suspended)
        {
            kraft::g_Renderer->PrepareFrame();
            kraft::g_Renderer->BeginMainRenderpass();

            // Draw stuff here!

            // Draw ImGui
            AppState.ImGuiRenderer.BeginFrame();
            ImGui::ShowDemoWindow(&ImGuiDemoWindowOpen);
            AppState.ImGuiRenderer.EndFrame();

            kraft::g_Renderer->EndMainRenderpass();
            AppState.ImGuiRenderer.EndFrameUpdatePlatformWindows();
        }

        if (AppState.TimeSinceLastFrame >= 1.f)
        {
            AppState.TimeSinceLastFrame = 0.f;
            AppState.FrameCount = 0;

            kraft::StringFormat(AppState.TitleBuffer, sizeof(AppState.TitleBuffer), "SampleApp | FrameTime: %.2f ms | %d fps", DeltaTime * 1000.f, AppState.FrameCount);
            Window->SetWindowTitle(AppState.TitleBuffer);
        }
    }

    // kraft::DestroyRenderer(Renderer);
    // kraft::DestroyWindow(Window);
    kraft::DestroyEngine();

    return 0;
}
