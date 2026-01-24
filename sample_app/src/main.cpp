#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>

#include <kraft.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/vulkan/kraft_vulkan_imgui.h>
#include <systems/kraft_asset_database.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>

#include <core/kraft_base_includes.h>
#include <platform/kraft_platform_includes.h>
#include <misc/kraft_misc_includes.h>

#include <kraft_types.h>
#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_asset_types.h>

#include <time.h>

using namespace kraft;

#include "imgui/imgui_renderer.h"

#include "imgui/imgui_renderer.cpp"

struct ApplicationState
{
    char          title_buffer[1024];
    f64           last_time;
    f64           time_since_last_frame;
    RendererImGui imgui_renderer;
    u32           frame_count;
} app_state;

struct GameState
{
    ArenaAllocator* arena;
    Mat4f           projection_matrix;
};

static void DrawUI(GameState* game_state)
{}

static bool OnWindowResize(EventType type, void* sender, void* listener, EventData event_data)
{
    GameState*      game_state = (GameState*)listener;
    EventDataResize data = *(EventDataResize*)(&event_data);
    game_state->projection_matrix = OrthographicMatrix(-(float)data.width * 0.5f, (float)data.width * 0.5f, -(float)data.height * 0.5f, (float)data.height * 0.5f, -1.0f, 1.0f);

    KDEBUG("Window resized to %d x %d", data.width, data.height);
    return false;
}

static bool OnKeyPress(EventType type, void* sender, void* listener, EventData event_data)
{
    // GameState* game_state = (GameState*)listener;
    // if (event_data.Int32Value[0] == Keys::KEY_B)
    // {
    // }

    return false;
}

int main(int argc, char** argv)
{
    ThreadContext* thread_context = CreateThreadContext();
    SetCurrentThreadContext(thread_context);

    srand(time(NULL));
    ArenaAllocator* arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(64),
        .Alignment = KRAFT_SIZE_KB(64),
    });

    GameState* game_state = ArenaPush(arena, GameState);
    game_state->arena = arena;

    bool                ImGuiDemoWindowOpen = true;
    kraft::EngineConfig config = {
        .argc = argc,
        .argv = argv,
        .application_name = String8Raw("SampleApp"),
        .console_app = false,
    };
    kraft::CreateEngine(&config);

    EventSystem::Listen(EVENT_TYPE_WINDOW_RESIZE, game_state, OnWindowResize);
    EventSystem::Listen(EVENT_TYPE_KEY_UP, game_state, OnKeyPress);

    // auto Window = kraft::CreateWindow("SampleApp", 1280, 768);
    auto window = CreateWindow({ .Title = "SampleApp", .Width = 1280, .Height = 768, .StartMaximized = true });
    CreateRenderer({
        .Backend = kraft::RENDERER_BACKEND_TYPE_VULKAN,
        .MaterialBufferSize = 32,
    });

    // Window->Maximize();

    app_state.imgui_renderer.Init(game_state->arena);

    auto material = MaterialSystem::CreateMaterialFromFile(S("res/materials/simple_2d.kmt"), r::Handle<r::RenderPass>::Invalid());
    KASSERT(material);

    auto  geometry = GeometrySystem::GetDefault2DGeometry();
    Mat4f bg_transform = ScaleMatrix(Vec3f{ 2560.0f, 1080.0f, 1.f }) * TranslationMatrix(Vec3f{ 0.f, 0.f, 0.f });

    r::GlobalShaderData global_ubo = {};
    game_state->projection_matrix = OrthographicMatrix(-(float)window->Width * 0.5f, (float)window->Width * 0.5f, -(float)window->Height * 0.5f, (float)window->Height * 0.5f, -1.0f, 1.0f);
    // game_state->projection_matrix = kraft::PerspectiveMatrix(kraft::DegToRadians(45.0f), (float)Window->Width / (float)Window->Height, 0.1f, 1000.f);
    global_ubo.View = Mat4f(kraft::Identity);

    while (Engine::running)
    {
        TempArena scratch = ScratchBegin(&game_state->arena, 1);
        app_state.frame_count++;

        f64 FrameStartTime = Platform::GetAbsoluteTime();
        f64 DeltaTime = FrameStartTime - app_state.last_time;
        Time::delta_time = DeltaTime;
        app_state.time_since_last_frame += DeltaTime;
        app_state.last_time = FrameStartTime;

        // Poll events
        Engine::Tick();

        // We only want to render when the engine is not suspended
        if (!kraft::Engine::suspended)
        {
            kraft::g_Renderer->PrepareFrame();
            kraft::g_Renderer->BeginMainRenderpass();

            // Draw stuff here!
            g_Renderer->AddRenderable({
                .ModelMatrix = bg_transform,
                .MaterialInstance = material,
                .GeometryId = geometry->InternalID,
                .EntityId = geometry->InternalID,
            });

            global_ubo.Projection = game_state->projection_matrix;
            kraft::g_Renderer->Draw(&global_ubo);

            // Draw ImGui
            app_state.imgui_renderer.BeginFrame();
            ImGui::ShowDemoWindow(&ImGuiDemoWindowOpen);
            DrawUI(game_state);
            app_state.imgui_renderer.EndFrame();

            kraft::g_Renderer->EndMainRenderpass();
            app_state.imgui_renderer.EndFrameUpdatePlatformWindows();
        }

        if (app_state.time_since_last_frame >= 1.f)
        {
            app_state.time_since_last_frame = 0.f;
            app_state.frame_count = 0;

            String8 window_title = StringFormat(scratch.arena, "SampleApp | FrameTime: %.2f ms | %d fps", DeltaTime * 1000.f, app_state.frame_count);
            window->SetWindowTitle(window_title.str);
        }

        ScratchEnd(scratch);
    }

    DestroyEngine();

    return 0;
}
