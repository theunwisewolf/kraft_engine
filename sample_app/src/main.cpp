#include "core/kraft_math.h"

#include <kraft.h>

#include <core/kraft_base_includes.h>
#include <containers/kraft_containers_includes.h>
#include <platform/kraft_platform_includes.h>
#include <misc/kraft_misc_includes.h>
#include <renderer/kraft_renderer_includes.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_systems_includes.h>

#include <kraft_types.h>

#include <time.h>

using namespace kraft;

#include <renderer/kraft_debug_camera.h>

#include "imgui/imgui_renderer.h"
#include "imgui/imgui_renderer.cpp"

struct ApplicationState {
    f64 last_time;
    f64 time_since_last_frame;
    RendererImGui imgui_renderer;
    u32 frame_count;
} app_state;

struct GameState {
    ArenaAllocator* arena;
    Mat4f projection_matrix;
};

static void DrawUI(GameState* game_state) {}

static bool OnWindowResize(EventType type, void* sender, void* listener, EventData event_data) {
    GameState* game_state = (GameState*)listener;
    EventDataResize data = *(EventDataResize*)(&event_data);
    game_state->projection_matrix = OrthographicMatrix(-(float)data.width * 0.5f, (float)data.width * 0.5f, -(float)data.height * 0.5f, (float)data.height * 0.5f, -1.0f, 1.0f);
    game_state->projection_matrix = PerspectiveMatrix(DegToRadians(45.0f), (float)data.width / (float)data.height, 0.1f, 1000.f);
    KDEBUG("Window resized to %d x %d", data.width, data.height);
    return false;
}

static bool OnKeyPress(EventType type, void* sender, void* listener, EventData event_data) {
    // GameState* game_state = (GameState*)listener;
    // if (event_data.Int32Value[0] == Keys::KEY_B)
    // {
    // }

    return false;
}

int main(int argc, char** argv) {
    static DebugCamera debug_camera;

    // Directional "sun" light
    static Vec3f sun_direction = Vec3f{-0.43f, -0.86f, -0.26f}; // roughly normalized (-0.5, -1.0, -0.3)
    static Vec3f sun_color = Vec3f{1.0f, 0.95f, 0.8f};
    static f32 sun_intensity = 1.0f;
    static f32 sun_ortho_size = 20.0f;
    static f32 sun_near = 0.1f;
    static f32 sun_far = 50.0f;
    static f32 sun_distance = 20.0f;

    ThreadContext* thread_context = CreateThreadContext();
    SetCurrentThreadContext(thread_context);

    srand(time(NULL));
    ArenaAllocator* arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(64),
        .Alignment = KRAFT_SIZE_KB(64),
    });

    GameState* game_state = ArenaPush(arena, GameState);
    game_state->arena = arena;

    bool imgui_demo_window_open = true;
    EngineConfig config = {
        .argc = argc,
        .argv = argv,
        .application_name = S("SampleApp"),
        .console_app = false,
    };
    CreateEngine(&config);

    EventSystem::Listen(EVENT_TYPE_WINDOW_RESIZE, game_state, OnWindowResize);
    EventSystem::Listen(EVENT_TYPE_KEY_UP, game_state, OnKeyPress);

    auto window = CreateWindow({.Title = "SampleApp", .Width = 1280, .Height = 768, .StartMaximized = true});
    CreateRenderer({
        .Backend = RENDERER_BACKEND_TYPE_VULKAN,
        .MaterialBufferSize = 64,
        // .MSAASamples = 8,
    });

    // Enable shader hot-reloading
    ShaderSystem::EnableHotReload(arena, S("../../tools/shader_compiler/bin/Debug/KraftShaderCompiler.exe"), S("res/shaders"));

    // Window->Maximize();

    debug_camera.Init(Vec3f{7.6f, 5.0f, 0.0f}, -33.0f, 180.0f);

    app_state.imgui_renderer.Init(game_state->arena);

    auto gltf_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/simple_3d.kmt"));
    auto debug_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/debug_material.kmt"));
    auto mesh = AssetDatabase::LoadMesh(arena, S("res/meshes/Box.gltf"));
    KASSERT(mesh);

    auto plane = g_Renderer->CreatePlaneGeometry(10.0f);
    Mat4f plane_model = TranslationMatrix(Vec3f{0.0f, -1.0f, 0.0f}) * ScaleMatrix(Vec3f{20.0f, 1.0f, 20.0f});

    // game_state->projection_matrix = OrthographicMatrix(-(float)window->Width * 0.5f, (float)window->Width * 0.5f, -(float)window->Height * 0.5f, (float)window->Height * 0.5f, -1.0f, 1.0f);
    game_state->projection_matrix = PerspectiveMatrix(DegToRadians(45.0f), (float)window->Width / (float)window->Height, 0.1f, 1000.f);

    // Create a render surface with only the depth buffer for the shadow pass
    r::RenderSurface shadow_pass_depth_surface = g_Renderer->CreateRenderSurface(S("ShadowPass"), window->Width, window->Height, false, true, true);

    auto imgui_texture = app_state.imgui_renderer.AddTexture(shadow_pass_depth_surface.depth_pass, shadow_pass_depth_surface.texture_sampler);

    r::BindGroupEntry shadow_entries[] = {
        {
            .texture = shadow_pass_depth_surface.depth_pass,
            .sampler = shadow_pass_depth_surface.texture_sampler,
            .binding = 0,
            .type = r::ResourceType::Sampler,
        },
    };
    auto shadow_bind_group = r::ResourceManager->CreateBindGroup({
        .debug_name = "ShadowMapBindGroup",
        .entries = shadow_entries,
        .entry_count = 1,
        .set_index = 3,
    });

    f64 angle = 0.0f;
    f64 last_angle = 0.0f;
    f32 speed = 2.0f;

    // Array of objects [  ]

    while (Engine::running) {
        TempArena scratch = ScratchBegin(&game_state->arena, 1);
        app_state.frame_count++;

        f64 frame_start_time = Platform::GetAbsoluteTime();
        f64 delta_time = frame_start_time - app_state.last_time;
        Time::delta_time = delta_time;
        app_state.time_since_last_frame += delta_time;
        app_state.last_time = frame_start_time;

        // Poll events
        Engine::Tick();
        ShaderSystem::ProcessHotReload();

        // We only want to render when the engine is not suspended
        if (!Engine::suspended) {
            // f32 final_angle = ((Sin(DegToRadians(angle)) * Sin(DegToRadians(angle))) / 100.0f /** 0.5f + 0.5f*/) * 360.0f;
            last_angle = angle;
            // constexpr f32 x = 1 / 60.0f;
            angle += (f64)speed * math::Clamp((f32)Time::delta_time, 0.0f, 0.016666668f);
            f32 final_angle = angle;

            Camera* camera = debug_camera.Update((f32)delta_time, (float)window->Width / (float)window->Height);
            g_Renderer->Camera = camera;
            g_Renderer->PrepareFrame();

            // Off-screen shadow pass (render from the sun's perspective)
            Vec3f sun_position = -sun_direction * sun_distance;
            Mat4f light_projection = OrthographicMatrix(-sun_ortho_size, sun_ortho_size, -sun_ortho_size, sun_ortho_size, sun_near, sun_far);
            Mat4f light_view = LookAt(sun_position, Vec3f{0.0f, 0.0f, 0.0f}, Vec3f{0.0f, 1.0f, 0.0f});
            Mat4f light_vp = light_view * light_projection;

            r::GlobalShaderData shadow_data = {};
            shadow_data.ViewProjection = light_vp;
            shadow_pass_depth_surface.Begin(scratch.arena, shadow_data);
            for (i32 i = 0; i < mesh->SubMeshes.Length; i++) {
                shadow_pass_depth_surface.Submit({
                    .ModelMatrix = ScaleMatrix(vec3{1.0f, 1.0f, 1.0f}),
                    .MaterialInstance = gltf_material,
                    .DrawData = mesh->SubMeshes[i].Geometry->DrawData,
                    .EntityId = mesh->SubMeshes[i].Geometry->ID,
                });
            }

            for (i32 i = 0; i < mesh->SubMeshes.Length; i++) {
                shadow_pass_depth_surface.Submit({
                    .ModelMatrix = ScaleMatrix(vec3{1.0f, 1.0f, 1.0f}) * RotationMatrixFromEulerAngles(vec3{0.f, final_angle, 0.f}) * TranslationMatrix(Vec3f{2.0f, 0.0f, 0.0f}),
                    .MaterialInstance = gltf_material,
                    .DrawData = mesh->SubMeshes[i].Geometry->DrawData,
                    .EntityId = mesh->SubMeshes[i].Geometry->ID,
                });
            }

            shadow_pass_depth_surface.Submit({
                .ModelMatrix = plane_model,
                .MaterialInstance = gltf_material,
                .DrawData = plane->DrawData,
                .EntityId = plane->ID,
            });
            shadow_pass_depth_surface.End();

            // Main swapchain pass
            g_Renderer->BeginMainRenderpass();

            r::RenderSurface* main_surface = g_Renderer->GetMainSurface();
            r::GlobalShaderData main_data = {};
            main_data.ViewProjection = camera->GetViewMatrix() * camera->ProjectionMatrix;
            main_data.LightViewProjection = light_vp;
            main_data.GlobalLightPosition = sun_position;
            main_data.GlobalLightColor = Vec4f{sun_color.x * sun_intensity, sun_color.y * sun_intensity, sun_color.z * sun_intensity, 1.0f};
            main_data.CameraPosition = camera->Position;
            main_data.Time = Time::delta_time;

            main_surface->Begin(scratch.arena, main_data);
            for (i32 i = 0; i < mesh->SubMeshes.Length; i++) {
                main_surface->Submit({
                    .ModelMatrix = ScaleMatrix(vec3{1.0f, 1.0f, 1.0f}),
                    .MaterialInstance = gltf_material,
                    .DrawData = mesh->SubMeshes[i].Geometry->DrawData,
                    .EntityId = mesh->SubMeshes[i].Geometry->ID,
                    .CustomBindGroup = shadow_bind_group,
                });
            }

            for (i32 i = 0; i < mesh->SubMeshes.Length; i++) {
                main_surface->Submit({
                    .ModelMatrix = ScaleMatrix(vec3{1.0f, 1.0f, 1.0f}) * RotationMatrixFromEulerAngles(vec3{0.f, final_angle, 0.f}) * TranslationMatrix(vec3{2.0f, 0.0f, 0.0f}),
                    .MaterialInstance = gltf_material,
                    .DrawData = mesh->SubMeshes[i].Geometry->DrawData,
                    .EntityId = mesh->SubMeshes[i].Geometry->ID,
                    .CustomBindGroup = shadow_bind_group,
                });
            }

            main_surface->Submit({
                .ModelMatrix = plane_model,
                .MaterialInstance = debug_material,
                .DrawData = plane->DrawData,
                .EntityId = plane->ID,
                .CustomBindGroup = shadow_bind_group,
            });
            main_surface->End();

#if 1
            // Draw ImGui
            app_state.imgui_renderer.BeginFrame();
            // ImGui::ShowDemoWindow(&imgui_demo_window_open);
            DrawUI(game_state);

            ImGui::Begin("Camera Debug");
            ImGui::DragFloat3("Position", debug_camera.target_position._data);
            ImGui::DragFloat("Pitch", &debug_camera.target_pitch);
            ImGui::DragFloat("Yaw", &debug_camera.target_yaw);
            ImGui::DragFloat("Move Speed", &debug_camera.move_speed, 0.5f, 0.1f, 100.0f);
            ImGui::DragFloat("Smoothing", &debug_camera.smoothing, 0.5f, 1.0f, 50.0f);
            ImGui::End();

            ImGui::Begin("Sun Light");
            ImGui::DragFloat3("Direction", sun_direction._data, 0.01f, -1.0f, 1.0f);
            sun_direction = Normalize(sun_direction);
            ImGui::ColorEdit3("Color", sun_color._data);
            ImGui::DragFloat("Intensity", &sun_intensity, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Ortho Size", &sun_ortho_size, 0.5f, 1.0f, 100.0f);
            ImGui::DragFloat("Distance", &sun_distance, 0.5f, 1.0f, 100.0f);
            ImGui::DragFloat("Near", &sun_near, 0.1f, 0.01f, 10.0f);
            ImGui::DragFloat("Far", &sun_far, 0.5f, 1.0f, 200.0f);
            ImGui::End();

            ImGui::Begin("ShadowPass");
            ImGui::Image(imgui_texture, {(f32)window->Width / 2.0f, (f32)window->Height / 2.0f}, {0, 1}, {1, 0});
            ImGui::End();

            ImGui::Begin("Cube Animation");
            ImGui::DragFloat("Speed", &speed);
            ImGui::Text("Jump %f", (angle - last_angle));
            ImGui::Text("Delta time %f", Time::delta_time);
            ImGui::End();
            app_state.imgui_renderer.EndFrame();
#endif
            g_Renderer->EndMainRenderpass();

#if 1
            app_state.imgui_renderer.EndFrameUpdatePlatformWindows();
#endif
        }

        if (app_state.time_since_last_frame >= 1.f) {
            String8 window_title = StringFormat(scratch.arena, "Playground | FrameTime: %.2f ms | %d fps", delta_time * 1000.f, app_state.frame_count);
            window->SetWindowTitle(window_title.str);

            app_state.time_since_last_frame = 0.f;
            app_state.frame_count = 0;
        }

        ScratchEnd(scratch);
    }

    DestroyEngine();

    return 0;
}
