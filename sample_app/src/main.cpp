#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_core.h>
#include <core/kraft_engine.h>
#include <core/kraft_log.h>
#include <core/kraft_time.h>
#include <kraft.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <renderer/kraft_renderer_frontend.h>
#include <systems/kraft_asset_database.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>

#include <kraft_types.h>
#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_asset_types.h>

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

    // auto Window = kraft::CreateWindow("SampleApp", 1280, 768);
    auto Window = kraft::CreateWindow({ .Title = "SampleApp", .Width = 1280, .Height = 768, .StartMaximized = true });
    auto Renderer = kraft::CreateRenderer({
        .Backend = kraft::RENDERER_BACKEND_TYPE_VULKAN,
    });

    // Window->Maximize();

    AppState.ImGuiRenderer.Init();

    // auto Shader = kraft::ShaderSystem::AcquireShader("res/shaders/basic.kfx.bkfx", kraft::renderer::Handle<kraft::renderer::RenderPass>::Invalid());
    auto MeshAsset = kraft::AssetDatabase::LoadMesh("res/meshes/viking_room.obj");
    auto Material = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_3d.kmt", kraft::renderer::Handle<kraft::renderer::RenderPass>::Invalid());
    KASSERT(Material);
    auto Length = MeshAsset->SubMeshes.Length;
    KASSERT(Length);
    auto Geometry = MeshAsset->SubMeshes[0].Geometry; //kraft::GeometrySystem::GetDefaultGeometry();

    kraft::renderer::GlobalShaderData GlobalData = {};
    GlobalData.Projection = kraft::OrthographicMatrix(-(float)Window->Width * 0.5f, (float)Window->Width * 0.5f, -(float)Window->Height * 0.5f, (float)Window->Height * 0.5f, -1.0f, 1.0f);
    // GlobalData.Projection = kraft::PerspectiveMatrix(kraft::DegToRadians(45.0f), (float)Window->Width / (float)Window->Height, 0.1f, 1000.f);
    GlobalData.View = kraft::Mat4f(kraft::Identity);

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
            kraft::g_Renderer->Draw(Material->Shader, &GlobalData, Geometry->InternalID);

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
