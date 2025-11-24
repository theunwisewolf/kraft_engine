#include <containers/kraft_array.h>
#include <containers/kraft_hashmap.h>
#include <core/kraft_core.h>
#include <core/kraft_events.h>
#include <core/kraft_input.h>
#include <core/kraft_log.h>
#include <kraft.h>
#include <platform/kraft_filesystem.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/vulkan/kraft_vulkan_imgui.h>
#include <systems/kraft_asset_database.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>

#include <core/kraft_base_includes.h>

#include <kraft_types.h>
#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_asset_types.h>

#include <time.h>

using namespace kraft;

#include "imgui/imgui_renderer.h"

#include "imgui/imgui_renderer.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct ApplicationState
{
    char          title_buffer[1024];
    float64       last_time;
    float64       time_since_last_frame;
    RendererImGui imgui_renderer;
    u32           frame_count;
} app_state;

struct UIParams
{
    u32 window_flags;
};

struct GameState
{
    ArenaAllocator* arena;
    UIParams        ui_params;
    Mat4f           projection_matrix;
};

void DrawGameUI(GameState* state)
{
    TempArena scratch = ScratchBegin(&state->arena, 1);
    ScratchEnd(scratch);
}

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
    GameState* game_state = (GameState*)listener;
    if (event_data.Int32Value[0] == Keys::KEY_B)
    {
        game_state->ui_params.window_flags ^= ImGuiWindowFlags_NoBackground;
    }

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

    bool         imgui_demo_window = true;
    EngineConfig config = {
        .argc = argc,
        .argv = argv,
        .application_name = String8Raw("TextDrawing"),
        .console_app = false,
    };
    CreateEngine(&config);

    EventSystem::Listen(EVENT_TYPE_WINDOW_RESIZE, game_state, OnWindowResize);
    EventSystem::Listen(EVENT_TYPE_KEY_UP, game_state, OnKeyPress);

    // auto Window = CreateWindow("SampleApp", 1280, 768);
    auto window = CreateWindow({ .Title = "TextDrawing", .Width = 1280, .Height = 768, .StartMaximized = true });
    auto _ = CreateRenderer({
        .Backend = RENDERER_BACKEND_TYPE_VULKAN,
    });

    // Window->Maximize();

    app_state.imgui_renderer.Init(game_state->arena);

    // Load the font
    const int size = 100;
    auto      hack_regular = fs::ReadAllBytes(game_state->arena, String8Raw("res/fonts/Hack-Regular.ttf"));
    KASSERT(hack_regular.ptr);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, hack_regular.ptr, stbtt_GetFontOffsetForIndex(hack_regular.ptr, 0));

    char A = 'A';
    // int  glyph_index = stbtt_FindGlyphIndex(&font, text);
    int width;
    int height;
    u8* bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, size), A, &width, &height, 0, 0);
    KASSERT(bitmap);

    auto texture = TextureSystem::AcquireTextureWithData(String8Raw("hack"), bitmap, width, height, 1);
    KASSERT(texture.IsInvalid() == false);

    auto bg_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/font.kmt"), r::Handle<r::RenderPass>::Invalid());
    KASSERT(bg_material);

    MaterialSystem::SetTexture(bg_material, String8Raw("DiffuseTexture"), texture);

    auto geometry = GeometrySystem::GetDefault2DGeometry();

    r::GlobalShaderData global_data = {};
    game_state->projection_matrix = OrthographicMatrix(-(f32)window->Width * 0.5f, (f32)window->Width * 0.5f, -(f32)window->Height * 0.5f, (f32)window->Height * 0.5f, -1.0f, 1.0f);
    // game_state->projection_matrix = PerspectiveMatrix(DegToRadians(45.0f), (float)Window->Width / (float)Window->Height, 0.1f, 1000.f);
    global_data.View = Mat4f(Identity);

    Mat4f transformA = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ -size / 2.f, 0.f, 0.f });
    Mat4f transformB = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ size / 2.f, 0.f, 0.f });

    while (Engine::running)
    {
        TempArena scratch = ScratchBegin(&game_state->arena, 1);
        app_state.frame_count++;

        f64 frame_start_time = Platform::GetAbsoluteTime();
        f64 delta_time = frame_start_time - app_state.last_time;
        Time::delta_time = delta_time;
        app_state.time_since_last_frame += delta_time;
        app_state.last_time = frame_start_time;

        // Poll events
        Engine::Tick();

        // We only want to render when the engine is not suspended
        if (!Engine::suspended)
        {
            g_Renderer->PrepareFrame();
            g_Renderer->BeginMainRenderpass();

            // Draw stuff here!
            global_data.Projection = game_state->projection_matrix;

            // f32 y = Sin(Time::elapsed_time);
            // KDEBUG("y = %f", y);
            transformA = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ -size / 2.f, Sin(Time::elapsed_time * 2.0f) * 100.0f, 0.f });
            transformB = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ size / 2.f, Sin(Time::elapsed_time * 2.f + 100.0f) * 100.0f, 0.f });

            g_Renderer->AddRenderable({ .EntityId = 1, .GeometryId = geometry->InternalID, .MaterialInstance = bg_material, .ModelMatrix = transformA });
            g_Renderer->AddRenderable({ .EntityId = 2, .GeometryId = geometry->InternalID, .MaterialInstance = bg_material, .ModelMatrix = transformB });
            // g_Renderer->Draw(bg_material->Shader, &global_data, geometry->InternalID);
            g_Renderer->Draw(&global_data);

            // Draw ImGui
            app_state.imgui_renderer.BeginFrame();
            ImGui::ShowDemoWindow(&imgui_demo_window);
            DrawGameUI(game_state);
            app_state.imgui_renderer.EndFrame();

            g_Renderer->EndMainRenderpass();
            app_state.imgui_renderer.EndFrameUpdatePlatformWindows();
        }

        if (app_state.time_since_last_frame >= 1.f)
        {
            String8 window_title = StringFormat(scratch.arena, "TextDrawing | FrameTime: %.2f ms | %d fps", delta_time * 1000.f, app_state.frame_count);
            window->SetWindowTitle(window_title.str);

            app_state.time_since_last_frame = 0.f;
            app_state.frame_count = 0;
        }

        ScratchEnd(scratch);
    }

    // DestroyRenderer(Renderer);
    // DestroyWindow(Window);
    DestroyEngine();

    return 0;
}
