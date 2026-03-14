#include "core/kraft_math.h"

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
#include <renderer/kraft_camera.h>

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
    game_state->projection_matrix = PerspectiveMatrix(DegToRadians(45.0f), (float)data.width / (float)data.height, 0.1f, 1000.f);
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
    static Vec3f camera_position = Vec3f{ -5.0f, 0.0f, -10.0f };
    static Vec3f camera_rotation = Vec3f{ 0.0f, 0.0f, 0.0f };

    ThreadContext* thread_context = CreateThreadContext();
    SetCurrentThreadContext(thread_context);

    srand(time(NULL));
    ArenaAllocator* arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(64),
        .Alignment = KRAFT_SIZE_KB(64),
    });

    GameState* game_state = ArenaPush(arena, GameState);
    game_state->arena = arena;

    bool         ImGuiDemoWindowOpen = true;
    EngineConfig config = {
        .argc = argc,
        .argv = argv,
        .application_name = String8Raw("SampleApp"),
        .console_app = false,
    };
    CreateEngine(&config);

    EventSystem::Listen(EVENT_TYPE_WINDOW_RESIZE, game_state, OnWindowResize);
    EventSystem::Listen(EVENT_TYPE_KEY_UP, game_state, OnKeyPress);

    // auto Window = kraft::CreateWindow("SampleApp", 1280, 768);
    auto window = CreateWindow({ .Title = "SampleApp", .Width = 1280, .Height = 768, .StartMaximized = true });
    CreateRenderer({
        .Backend = RENDERER_BACKEND_TYPE_VULKAN,
        .MaterialBufferSize = 64,
    });

    // Window->Maximize();

    auto camera = Camera();

    app_state.imgui_renderer.Init(game_state->arena);

    auto gltf_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/simple_3d.kmt"));
    auto mesh = AssetDatabase::LoadMesh(arena, S("res/meshes/Box.gltf"));
    KASSERT(mesh);

    auto material = MaterialSystem::CreateMaterialFromFile(S("res/materials/simple_2d.kmt"));
    KASSERT(material);

    auto  geometry = GeometrySystem::GetDefault2DGeometry();
    Mat4f bg_transform = ScaleMatrix(Vec3f{ 2560.0f, 1080.0f, 1.f }) * TranslationMatrix(Vec3f{ 0.f, 0.f, 0.f });

    r::GlobalShaderData global_ubo = {};
    // game_state->projection_matrix = OrthographicMatrix(-(float)window->Width * 0.5f, (float)window->Width * 0.5f, -(float)window->Height * 0.5f, (float)window->Height * 0.5f, -1.0f, 1.0f);
    game_state->projection_matrix = PerspectiveMatrix(DegToRadians(45.0f), (float)window->Width / (float)window->Height, 0.1f, 1000.f);
    global_ubo.View = Mat4f(Identity);

    // Create a render surface with only the depth buffer for the shadow pass
    r::RenderSurface shadow_pass_depth_surface = g_Renderer->CreateRenderSurface(S("ShadowPass"), window->Width, window->Height, false, true, true);

    auto imgui_texture = app_state.imgui_renderer.AddTexture(shadow_pass_depth_surface.DepthPassTexture, shadow_pass_depth_surface.TextureSampler);

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
        if (!Engine::suspended)
        {
            camera.Position = camera_position;
            camera.Pitch = camera_rotation.x;
            camera.Yaw = camera_rotation.y;
            camera.Roll = camera_rotation.z;
            camera.SetPerspectiveProjection(DegToRadians(45.0f), (float)window->Width / (float)window->Height, 0.1f, 1000.f);
            camera.UpdateVectors();

            g_Renderer->Camera = &camera;
            g_Renderer->BeginRenderSurface(shadow_pass_depth_surface);
            for (i32 i = 0; i < mesh->SubMeshes.Length; i++)
            {
                g_Renderer->AddRenderable({
                    .ModelMatrix = ScaleMatrix(Vec3f{ 1.0f, 1.0f, 1.0f }),
                    .MaterialInstance = gltf_material,
                    .DrawData = mesh->SubMeshes[i].Geometry->DrawData,
                    .EntityId = mesh->SubMeshes[i].Geometry->ID,
                });
            }
            g_Renderer->EndRenderSurface(shadow_pass_depth_surface);

            shadow_pass_depth_surface.global_shader_data.View = game_state->projection_matrix;

            g_Renderer->PrepareFrame();
            g_Renderer->DrawSurfaces();
            g_Renderer->BeginMainRenderpass();

            // g_Renderer->AddRenderable({
            //     .ModelMatrix = bg_transform,
            //     .MaterialInstance = material,
            //     .GeometryId = geometry->InternalID,
            //     .EntityId = geometry->InternalID,
            // });

            for (i32 i = 0; i < mesh->SubMeshes.Length; i++)
            {
                g_Renderer->AddRenderable({
                    .ModelMatrix = ScaleMatrix(Vec3f{ 1.0f, 1.0f, 1.0f }),
                    .MaterialInstance = gltf_material,
                    .DrawData = mesh->SubMeshes[i].Geometry->DrawData,
                    .EntityId = mesh->SubMeshes[i].Geometry->ID,
                });
            }

            global_ubo.Projection = game_state->projection_matrix;
            global_ubo.View = RotationMatrixFromEulerAngles(camera_rotation) * TranslationMatrix(camera_position);
            g_Renderer->Draw(&global_ubo);

#if 1
            // Draw ImGui
            app_state.imgui_renderer.BeginFrame();
            // ImGui::ShowDemoWindow(&ImGuiDemoWindowOpen);
            DrawUI(game_state);

            ImGui::Begin("Camera Debug");
            ImGui::DragFloat3("Position", camera_position._data);
            ImGui::DragFloat3("Rotation", camera_rotation._data);
            ImGui::End();

            ImGui::Begin("ShadowPass");
            ImGui::Image(imgui_texture, { (f32)window->Width, (f32)window->Height }, { 0, 1 }, { 1, 0 });
            ImGui::End();
            app_state.imgui_renderer.EndFrame();
#endif
            g_Renderer->EndMainRenderpass();

#if 1
            app_state.imgui_renderer.EndFrameUpdatePlatformWindows();
#endif
        }

        if (app_state.time_since_last_frame >= 1.f)
        {
            app_state.time_since_last_frame = 0.f;
            app_state.frame_count = 0;

            String8 window_title = StringFormat(scratch.arena, "Playground | FrameTime: %.2f ms | %d fps", DeltaTime * 1000.f, app_state.frame_count);
            window->SetWindowTitle(window_title.str);
        }

        ScratchEnd(scratch);
    }

    DestroyEngine();

    return 0;
}
