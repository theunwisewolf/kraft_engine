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

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

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
        .MaterialBufferSize = 32,
    });

    // Window->Maximize();

    app_state.imgui_renderer.Init(game_state->arena);

    // Load the font
    const int size = 32;
    auto      hack_regular = fs::ReadAllBytes(game_state->arena, String8Raw("res/fonts/Hack-Regular.ttf"));
    KASSERT(hack_regular.ptr);

    auto font_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/font.kmt"), r::Handle<r::RenderPass>::Invalid());
    KASSERT(font_material);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, hack_regular.ptr, stbtt_GetFontOffsetForIndex(hack_regular.ptr, 0));

    // Create a texture with the characters that we need
    const u32          num_chars = '~' - ' ';
    r::Renderable      font_bitmap;
    stbtt_packedchar   packed_characters[num_chars];
    stbtt_aligned_quad aligned_quads[num_chars];
    {
        f32                width = 512;
        f32                height = 512;
        u8*                packed_bitmap = ArenaPushArray(arena, u8, width * height);
        stbtt_pack_context pack_context;
        stbtt_pack_range   range = {
              .font_size = size,
              .first_unicode_codepoint_in_range = ' ',
              .num_chars = num_chars,
              .chardata_for_range = packed_characters,
              .array_of_unicode_codepoints = nullptr,
              .h_oversample = 1,
              .v_oversample = 2,
        };
        stbtt_PackBegin(&pack_context, packed_bitmap, width, height, 0, 1, nullptr);
        // stbtt_PackSetOversampling(&pack_context, 2, 2);
        stbtt_PackFontRanges(&pack_context, font.data, 0, &range, 1);
        stbtt_PackEnd(&pack_context);

        f32 _x = 0.f;
        f32 _y = 0.f;
        for (u32 i = 0; i < num_chars; i++)
        {
            stbtt_GetPackedQuad(packed_characters, width, height, i, &_x, &_y, &aligned_quads[i], 0);
            KDEBUG("Quad for %c = { { %f, %f }, { %f, %f } }", ' ' + i, aligned_quads[i].s0, aligned_quads[i].t0, aligned_quads[i].s1, aligned_quads[i].t1);
        }

        KASSERT(packed_bitmap);

        auto bitmap_texture = TextureSystem::AcquireTextureWithData(String8Raw("bitmap"), packed_bitmap, width, height, 1);
        auto bitmap_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/simple_2d.kmt"), r::Handle<r::RenderPass>::Invalid());
        KASSERT(bitmap_material);

        MaterialSystem::SetTexture(bitmap_material, String8Raw("DiffuseTexture"), bitmap_texture);
        MaterialSystem::SetTexture(font_material, String8Raw("DiffuseTexture"), bitmap_texture);
        auto  geometry = GeometrySystem::GetDefault2DGeometry();
        Mat4f transform = ScaleMatrix(Vec3f{ width, height, 1.f }) * TranslationMatrix(Vec3f{ 1.f, 1.f, 0.f });
        font_bitmap.EntityId = 1;
        font_bitmap.GeometryId = geometry->InternalID;
        font_bitmap.MaterialInstance = bitmap_material;
        font_bitmap.ModelMatrix = transform;
    }

    // char A = 'A';
    // int  glyph_index = stbtt_FindGlyphIndex(&font, text);
    // int width;
    // int height;
    // u8* bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, size), A, &width, &height, 0, 0);
    // u8* bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, size), A, &width, &height, 0, 0);
    // KASSERT(bitmap);

    // auto texture = TextureSystem::AcquireTextureWithData(String8Raw("hack"), bitmap, width, height, 1);
    // KASSERT(texture.IsInvalid() == false);

    // MaterialSystem::SetTexture(font_material, String8Raw("DiffuseTexture"), texture);

    // auto geometry = GeometrySystem::GetDefault2DGeometry();
    // u16         indices[] = { 0, 1, 2, 2, 3, 0 };
    // r::Vertex2D vertices[] = {
    //     { .Position = { +0.5f, +0.5f, +0.0f }, .UV = { 1.f, 1.f } },
    //     { .Position = { +0.5f, -0.5f, +0.0f }, .UV = { 1.f, 0.f } },
    //     { .Position = { -0.5f, -0.5f, +0.0f }, .UV = { 0.f, 0.f } },
    //     { .Position = { -0.5f, +0.5f, +0.0f }, .UV = { 0.f, 1.f } },
    // };
    // auto geometry_custom = GeometrySystem::AcquireGeometryWithData({ .Vertices = vertices, .IndexCount = 6, .IndexSize = sizeof(u16), .Indices = indices });

    r::GlobalShaderData global_data = {};
    game_state->projection_matrix = OrthographicMatrix(-(f32)window->Width * 0.5f, (f32)window->Width * 0.5f, -(f32)window->Height * 0.5f, (f32)window->Height * 0.5f, -1.0f, 1.0f);
    // game_state->projection_matrix = PerspectiveMatrix(DegToRadians(45.0f), (float)Window->Width / (float)Window->Height, 0.1f, 1000.f);
    global_data.View = Mat4f(Identity);

    Mat4f transformA = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ -size / 2.f, 0.f, 0.f });
    Mat4f transformB = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ size / 2.f, 0.f, 0.f });

    // String8 text = String8Raw("#include <cstdio>\n#include <cstdlib>\n\nint main() {\n\tint x = 1;\n\tprintf(\"Hello from C! %d\");\n}");
    String8 text = String8Raw("This is rendered text!\nIt supports \nnewlines and \ttabs!\n01234567890");
    // String8      text = String8Raw("H");
    r::Vertex2D* vertices = ArenaPushArray(arena, r::Vertex2D, 4 * text.count);
    u32*         indices = ArenaPushArray(arena, u32, 6 * text.count);

    int ascent;
    stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
    // stbtt_GetFontVMetrics(&src_tmp.FontInfo, &unscaled_ascent, &unscaled_descent, &unscaled_line_gap);
    f32 scale = stbtt_ScaleForPixelHeight(&font, size);
    f32 space_advance = packed_characters[0].xadvance; // Space is the first character in our range
    f32 baseline_x = 0.f;
    f32 baseline_y = 0.f; // ascent * scale; // setting baseline_y to ascent * scale doesn't work for some reason
    // f32 baseline_y = 0.f;
    for (u32 i = 0; i < text.count; i++)
    {
        char character = text.ptr[i];
        if (character == '\n')
        {
            baseline_y -= ascent * scale;
            baseline_x = 0.f;
            continue;
        }

        if (character == '\t')
        {
            baseline_x += 4.0f * space_advance;
            continue;
        }

        u32                char_index = character - ' ';
        stbtt_packedchar   packed_char = packed_characters[char_index];
        stbtt_aligned_quad aligned_quad = aligned_quads[char_index];
        // f32                character_width = packed_char.x1 - packed_char.x0;
        // f32                character_height = -(packed_char.y1 - packed_char.y0);
        f32 character_width = (scale * aligned_quad.x1 - scale * aligned_quad.x0);
        f32 character_height = (scale * aligned_quad.y1 - scale * aligned_quad.y0);

        f32 kern_advance = 0.f;
        if ((i + 1) < text.count)
        {
            kern_advance = scale * stbtt_GetCodepointKernAdvance(&font, text.ptr[i], text.ptr[i + 1]);
        }

        // KDEBUG("Quad for %c = { { %f, %f }, { %f, %f } }", character, aligned_quads[char_index].s0, aligned_quads[char_index].t0, aligned_quads[char_index].s1, aligned_quads[char_index].t1);

        vertices[4 * i + 0] = r::Vertex2D{
            .Position = { baseline_x + packed_char.xoff2, baseline_y - packed_char.yoff, 0.f }, // top-right
            .UV = { aligned_quad.s1, aligned_quad.t0 },
        };
        vertices[4 * i + 1] = r::Vertex2D{
            .Position = { baseline_x + packed_char.xoff2, baseline_y - packed_char.yoff2, 0.f }, // bottom-right
            .UV = { aligned_quad.s1, aligned_quad.t1 },
        };
        vertices[4 * i + 2] = r::Vertex2D{
            .Position = { baseline_x + packed_char.xoff, baseline_y - packed_char.yoff2, 0.f }, // bottom-left
            .UV = { aligned_quad.s0, aligned_quad.t1 },
        };
        vertices[4 * i + 3] = r::Vertex2D{
            .Position = { baseline_x + packed_char.xoff, baseline_y - packed_char.yoff, 0.f }, // top-left
            .UV = { aligned_quad.s0, aligned_quad.t0 },
        };

        indices[6 * i + 0] = 4 * i + 0;
        indices[6 * i + 1] = 4 * i + 1;
        indices[6 * i + 2] = 4 * i + 2;
        indices[6 * i + 3] = 4 * i + 2;
        indices[6 * i + 4] = 4 * i + 3;
        indices[6 * i + 5] = 4 * i + 0;

        baseline_x += kern_advance + packed_char.xadvance;
    }

    auto geometry_data = GeometryData{
        .VertexCount = 4 * (u32)text.count,
        .IndexCount = 6 * (u32)text.count,
        .VertexSize = sizeof(r::Vertex2D),
        .IndexSize = sizeof(u32),
        .Vertices = vertices,
        .Indices = indices,
    };
    auto geometry_custom = GeometrySystem::AcquireGeometryWithData(geometry_data);

    MaterialSystem::SetProperty(font_material, String8Raw("DiffuseColor"), KRAFT_HEX(0xe74c3c));

    // Store original positions and phase offsets for each character
    r::Vertex2D* original_vertices = ArenaPushArray(arena, r::Vertex2D, 4 * text.count);
    MemCpy(original_vertices, vertices, sizeof(r::Vertex2D) * 4 * text.count);

    f32* phase_offsets = ArenaPushArray(arena, f32, text.count);
    for (u32 i = 0; i < text.count; i++)
    {
        phase_offsets[i] = ((f32)rand() / (f32)RAND_MAX) * 6.28318f; // 2*PI
    }

    f32 wave_amplitude = 3.0f;
    f32 wave_speed = 8.0f;

    srand(time(NULL));
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
            // transformB = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ size / 2.f, Sin(Time::elapsed_time * 2.f + 100.0f) * 100.0f, 0.f });
            transformB = ScaleMatrix(Vec3f{ 1.0f, 1.f, 1.f });

            for (u32 char_idx = 0; char_idx < text.count; char_idx++)
            {
                u32 vertex_start = 4 * char_idx;
                f32 y_offset = Sin(Time::elapsed_time * wave_speed + phase_offsets[char_idx]) * wave_amplitude;

                for (u32 j = 0; j < 4; j++)
                {
                    u32   vertex_idx = vertex_start + j;
                    Vec3f original_pos = original_vertices[vertex_idx].Position;
                    ((r::Vertex2D*)geometry_data.Vertices)[vertex_idx].Position = Vec3f{ original_pos.x, original_pos.y + y_offset, original_pos.z };
                }
            }

            GeometrySystem::UpdateGeometry(geometry_custom, geometry_data);

            // g_Renderer->AddRenderable({ .EntityId = 1, .GeometryId = geometry->InternalID, .MaterialInstance = font_material, .ModelMatrix = transformA });
            // g_Renderer->AddRenderable({ .EntityId = 2, .GeometryId = geometry->InternalID, .MaterialInstance = font_material, .ModelMatrix = transformB });
            g_Renderer->AddRenderable({
                .EntityId = 420,
                .GeometryId = geometry_custom->InternalID,
                .MaterialInstance = font_material,
                .ModelMatrix = transformB,
            });
            // g_Renderer->AddRenderable(font_bitmap);
            // g_Renderer->Draw(bg_material->Shader, &global_data, geometry->InternalID);
            g_Renderer->Draw(&global_data);

            // Draw ImGui
            app_state.imgui_renderer.BeginFrame();
            ImGui::ShowDemoWindow(&imgui_demo_window);
            DrawGameUI(game_state);

            ImGui::Begin("TextRenderDebug");
            ImGui::DragFloat("Wave amplitude", &wave_amplitude);
            ImGui::DragFloat("Wave speed", &wave_speed);
            ImGui::End();

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
