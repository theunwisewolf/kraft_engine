#include <kraft.h>

#include <core/kraft_base_includes.h>
#include <platform/kraft_platform_includes.h>
#include <containers/kraft_containers_includes.h>
#include <renderer/kraft_renderer_includes.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_systems_includes.h>

#include <misc/kraft_misc_includes.h>

#include <kraft_types.h>
#include <resources/kraft_resource_types.h>
#include <renderer/kraft_slug_text_renderer.h>

#include <time.h>

using namespace kraft;

#include "imgui/imgui_renderer.h"
#include "imgui/imgui_renderer.cpp"

struct AppFrameStats {
    u64 frame_id;
    bool rendered_frame;
    bool engine_suspended;
    f64 total_ms;
    f64 delta_time_ms;
    f64 scratch_begin_ms;
    f64 engine_tick_ms;
    f64 engine_time_update_ms;
    f64 engine_input_update_ms;
    f64 engine_poll_events_ms;
    f64 render_block_ms;
    f64 renderer_prepare_frame_ms;
    f64 renderer_begin_main_renderpass_ms;
    f64 scene_build_ms;
    f64 scene_surface_begin_ms;
    f64 scene_non_slug_update_ms;
    f64 scene_non_slug_submit_ms;
    f64 scene_static_slug_total_ms;
    f64 scene_static_slug_render_text_ms;
    f64 scene_static_slug_submit_ms;
    f64 scene_dynamic_slug_total_ms;
    f64 scene_dynamic_slug_render_text_ms;
    f64 scene_dynamic_slug_submit_ms;
    f64 scene_surface_end_ms;
    f64 imgui_total_ms;
    f64 imgui_begin_frame_ms;
    f64 imgui_build_ui_ms;
    f64 imgui_end_frame_ms;
    f64 renderer_end_main_renderpass_ms;
    f64 imgui_platform_windows_ms;
    f64 title_update_ms;
    f64 scratch_end_ms;
};

struct AppStats {
    AppFrameStats current_frame;
    AppFrameStats last_completed_frame;
    u64 frame_counter;
};

struct ApplicationState {
    char title_buffer[1024];
    f64 last_time;
    f64 time_since_last_frame;
    RendererImGui imgui_renderer;
    u32 frame_count;
    AppStats stats;
} app_state;

struct UIParams {
    u32 window_flags;
};

struct GameState {
    ArenaAllocator* arena;
    UIParams ui_params;
    Mat4f projection_matrix;
};

const char* static_character_array[] = {
    // A-Z
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    // a-z
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
    // 0-9
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    // Punctuation & symbols
    "!",
    "\"",
    "#",
    "$",
    "%",
    "&",
    "'",
    "(",
    ")",
    "*",
    "+",
    ",",
    "-",
    ".",
    "/",
    ":",
    ";",
    "<",
    "=",
    ">",
    "?",
    "@",
    "[",
    "\\",
    "]",
    "^",
    "_",
    "`",
    "{",
    "|",
    "}",
    "~"
};

static constexpr u64 CHAR_ARRAY_SIZE = sizeof(static_character_array) / sizeof(static_character_array[0]);

static String8 GetStaticRandomText(ArenaAllocator* arena, u16 length, u32 seed) {
    u8* buf = ArenaPushArray(arena, u8, length + 1);

    if (seed)
        srand(seed);
    for (u16 i = 0; i < length; i++) {
        buf[i] = static_character_array[rand() % CHAR_ARRAY_SIZE][0];
    }

    buf[length] = '\0';

    return {buf, length};
}

void DrawGameUI(GameState* state) {
    TempArena scratch = ScratchBegin(&state->arena, 1);
    ScratchEnd(scratch);
}

static bool OnWindowResize(EventType type, void* sender, void* listener, EventData event_data) {
    GameState* game_state = (GameState*)listener;
    EventDataResize data = *(EventDataResize*)(&event_data);
    game_state->projection_matrix = OrthographicMatrix(-(float)data.width * 0.5f, (float)data.width * 0.5f, -(float)data.height * 0.5f, (float)data.height * 0.5f, -1.0f, 1.0f);

    KDEBUG("Window resized to %d x %d", data.width, data.height);
    return false;
}

static bool OnKeyPress(EventType type, void* sender, void* listener, EventData event_data) {
    GameState* game_state = (GameState*)listener;
    if (event_data.Int32Value[0] == Keys::KEY_B) {
        game_state->ui_params.window_flags ^= ImGuiWindowFlags_NoBackground;
    }

    return false;
}

f32 lerp(f32 a, f32 b, f32 amount) {
    return a + (b - a) * amount;
};

struct Transform {
    Vec2f position;
    Vec2f scale;
};

Mat4f MatrixFromTransform(Transform transform) {
    return ScaleMatrix(Vec3f{transform.scale.x, transform.scale.y, 1.0f}) * TranslationMatrix(Vec3f{transform.position.x, transform.position.y, 0.0f});
}

static void BeginAppStatsFrame(AppStats* stats) {
    if (stats->current_frame.frame_id != 0) {
        stats->last_completed_frame = stats->current_frame;

        if (stats->current_frame.frame_id % 120 == 0) {
            const AppFrameStats& s = stats->current_frame;
            KINFO("=== App Frame Stats (frame %llu) ===", s.frame_id);
            KINFO("  Total: %.3f ms | DeltaTime: %.3f ms", s.total_ms, s.delta_time_ms);
            KINFO(
                "  ScratchBegin: %.3f | EngineTick: %.3f | RenderBlock: %.3f | TitleUpdate: %.3f | ScratchEnd: %.3f",
                s.scratch_begin_ms,
                s.engine_tick_ms,
                s.render_block_ms,
                s.title_update_ms,
                s.scratch_end_ms
            );
            KINFO("  Engine: time=%.3f input=%.3f poll=%.3f", s.engine_time_update_ms, s.engine_input_update_ms, s.engine_poll_events_ms);
            KINFO("  Renderer: prepare=%.3f beginPass=%.3f endPass=%.3f", s.renderer_prepare_frame_ms, s.renderer_begin_main_renderpass_ms, s.renderer_end_main_renderpass_ms);
            KINFO("  SceneBuild: %.3f ms", s.scene_build_ms);
            KINFO("    SurfaceBegin: %.3f", s.scene_surface_begin_ms);
            KINFO("    NonSlug: update=%.3f submit=%.3f", s.scene_non_slug_update_ms, s.scene_non_slug_submit_ms);
            KINFO("    StaticSlugGrid: total=%.3f renderText=%.3f submit=%.3f", s.scene_static_slug_total_ms, s.scene_static_slug_render_text_ms, s.scene_static_slug_submit_ms);
            KINFO("    DynamicSlug: total=%.3f renderText=%.3f submit=%.3f", s.scene_dynamic_slug_total_ms, s.scene_dynamic_slug_render_text_ms, s.scene_dynamic_slug_submit_ms);
            KINFO("    SurfaceEnd: %.3f", s.scene_surface_end_ms);
            KINFO(
                "  ImGui: total=%.3f begin=%.3f build=%.3f end=%.3f platformWin=%.3f", s.imgui_total_ms, s.imgui_begin_frame_ms, s.imgui_build_ui_ms, s.imgui_end_frame_ms, s.imgui_platform_windows_ms
            );
            KINFO("===================================");
            fflush(stdout);
        }
    }

    stats->current_frame = {};
    stats->current_frame.frame_id = ++stats->frame_counter;
}

static void DrawTimingStatLine(const char* label, f64 value_ms) {
    ImGui::Text("%s: %.3f ms", label, value_ms);
}

static void DrawByteStatLine(const char* label, u64 bytes) {
    ImGui::Text("%s: %.2f KiB (%llu bytes)", label, bytes / 1024.0, (unsigned long long)bytes);
}

static f64 ClampNonNegativeMs(f64 value_ms) {
    return value_ms > 0.0 ? value_ms : 0.0;
}

static void DrawUploadStatsTree(const char* label, const r::RendererUploadStats& stats) {
    if (ImGui::TreeNode(label)) {
        DrawTimingStatLine("Total", stats.total_ms);
        ImGui::Text("Calls: %u", stats.call_count);
        ImGui::Text("Blocks Uploaded: %u", stats.block_count);
        ImGui::Text("Touched Blocks: %u", stats.touched_block_count);
        DrawByteStatLine("Bytes Uploaded", stats.bytes_uploaded);
        DrawByteStatLine("Bytes Used", stats.bytes_used);
        ImGui::TreePop();
    }
}

static void DrawAppFrameStatsTree(const char* label, const AppFrameStats& stats) {
    if (stats.frame_id == 0) {
        ImGui::Text("%s: waiting for first completed frame", label);
        return;
    }

    if (ImGui::TreeNode(label)) {
        f64 top_level_known_ms = stats.scratch_begin_ms + stats.engine_tick_ms + stats.render_block_ms + stats.title_update_ms + stats.scratch_end_ms;
        f64 render_known_ms = stats.renderer_prepare_frame_ms + stats.renderer_begin_main_renderpass_ms + stats.scene_build_ms + stats.imgui_total_ms + stats.renderer_end_main_renderpass_ms +
                              stats.imgui_platform_windows_ms;
        f64 engine_known_ms = stats.engine_time_update_ms + stats.engine_input_update_ms + stats.engine_poll_events_ms;
        f64 scene_known_ms = stats.scene_surface_begin_ms + stats.scene_non_slug_update_ms + stats.scene_non_slug_submit_ms + stats.scene_static_slug_total_ms + stats.scene_dynamic_slug_total_ms +
                             stats.scene_surface_end_ms;
        f64 static_slug_known_ms = stats.scene_static_slug_render_text_ms + stats.scene_static_slug_submit_ms;
        f64 dynamic_slug_known_ms = stats.scene_dynamic_slug_render_text_ms + stats.scene_dynamic_slug_submit_ms;
        f64 imgui_known_ms = stats.imgui_begin_frame_ms + stats.imgui_build_ui_ms + stats.imgui_end_frame_ms;

        ImGui::Text("Frame: %llu", (unsigned long long)stats.frame_id);
        DrawTimingStatLine("App Frame Total", stats.total_ms);
        DrawTimingStatLine("Delta Time", stats.delta_time_ms);
        ImGui::Text("Rendered Frame: %s", stats.rendered_frame ? "true" : "false");
        ImGui::Text("Engine Suspended: %s", stats.engine_suspended ? "true" : "false");
        DrawTimingStatLine("ScratchBegin", stats.scratch_begin_ms);

        if (ImGui::TreeNode("Engine::Tick")) {
            DrawTimingStatLine("Total", stats.engine_tick_ms);
            DrawTimingStatLine("Time::Update", stats.engine_time_update_ms);
            DrawTimingStatLine("InputSystem::Update", stats.engine_input_update_ms);
            DrawTimingStatLine("Platform::PollEvents", stats.engine_poll_events_ms);
            DrawTimingStatLine("Unaccounted", ClampNonNegativeMs(stats.engine_tick_ms - engine_known_ms));
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Render Block")) {
            DrawTimingStatLine("Total", stats.render_block_ms);
            DrawTimingStatLine("PrepareFrame", stats.renderer_prepare_frame_ms);
            DrawTimingStatLine("BeginMainRenderpass", stats.renderer_begin_main_renderpass_ms);
            DrawTimingStatLine("EndMainRenderpass", stats.renderer_end_main_renderpass_ms);
            DrawTimingStatLine("ImGui Platform Windows", stats.imgui_platform_windows_ms);

            if (ImGui::TreeNode("Scene Build + Submit")) {
                DrawTimingStatLine("Total", stats.scene_build_ms);
                DrawTimingStatLine("Surface Begin", stats.scene_surface_begin_ms);
                DrawTimingStatLine("Non-Slug Update", stats.scene_non_slug_update_ms);
                DrawTimingStatLine("Non-Slug Submit", stats.scene_non_slug_submit_ms);

                if (ImGui::TreeNode("Static Slug Grid")) {
                    DrawTimingStatLine("Total", stats.scene_static_slug_total_ms);
                    DrawTimingStatLine("SlugRenderText", stats.scene_static_slug_render_text_ms);
                    DrawTimingStatLine("Submit", stats.scene_static_slug_submit_ms);
                    DrawTimingStatLine("Unaccounted", ClampNonNegativeMs(stats.scene_static_slug_total_ms - static_slug_known_ms));
                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Dynamic Slug Text")) {
                    DrawTimingStatLine("Total", stats.scene_dynamic_slug_total_ms);
                    DrawTimingStatLine("SlugRenderText", stats.scene_dynamic_slug_render_text_ms);
                    DrawTimingStatLine("Submit", stats.scene_dynamic_slug_submit_ms);
                    DrawTimingStatLine("Unaccounted", ClampNonNegativeMs(stats.scene_dynamic_slug_total_ms - dynamic_slug_known_ms));
                    ImGui::TreePop();
                }

                DrawTimingStatLine("Surface End", stats.scene_surface_end_ms);
                DrawTimingStatLine("Unaccounted", ClampNonNegativeMs(stats.scene_build_ms - scene_known_ms));
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("ImGui")) {
                DrawTimingStatLine("Total", stats.imgui_total_ms);
                DrawTimingStatLine("BeginFrame", stats.imgui_begin_frame_ms);
                DrawTimingStatLine("Build UI", stats.imgui_build_ui_ms);
                DrawTimingStatLine("EndFrame", stats.imgui_end_frame_ms);
                DrawTimingStatLine("Unaccounted", ClampNonNegativeMs(stats.imgui_total_ms - imgui_known_ms));
                ImGui::TreePop();
            }

            DrawTimingStatLine("Unaccounted", ClampNonNegativeMs(stats.render_block_ms - render_known_ms));
            ImGui::TreePop();
        }

        DrawTimingStatLine("Title Update", stats.title_update_ms);
        DrawTimingStatLine("ScratchEnd", stats.scratch_end_ms);
        DrawTimingStatLine("Unaccounted", ClampNonNegativeMs(stats.total_ms - top_level_known_ms));
        ImGui::TreePop();
    }
}

static void DrawRendererFrameStatsTree(const char* label, const r::RendererFrameStats& stats) {
    if (stats.frame_id == 0) {
        ImGui::Text("%s: waiting for first completed frame", label);
        return;
    }

    if (ImGui::TreeNode(label)) {
        ImGui::Text("Frame: %llu", (unsigned long long)stats.frame_id);
        DrawTimingStatLine("Renderer Total", stats.total_ms);

        if (ImGui::TreeNode("PrepareFrame")) {
            DrawTimingStatLine("Total", stats.prepare_frame.total_ms);
            DrawTimingStatLine("ResourceManager::EndFrame", stats.prepare_frame.resource_end_frame_ms);
            DrawTimingStatLine("Backend PrepareFrame", stats.prepare_frame.backend_prepare_frame_ms);
            DrawTimingStatLine("Material Staging MemCpy", stats.prepare_frame.material_stage_memcpy_ms);
            DrawTimingStatLine("Material Upload", stats.prepare_frame.material_upload_ms);
            DrawTimingStatLine("Texture Updates", stats.prepare_frame.texture_update_ms);
            DrawTimingStatLine("Allocator Reset", stats.prepare_frame.allocator_reset_ms);
            ImGui::Text("Materials Uploaded: %s", stats.prepare_frame.materials_uploaded ? "true" : "false");
            ImGui::Text("Dirty Textures: %u", stats.prepare_frame.dirty_texture_count);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Dynamic Geometry")) {
            DrawTimingStatLine("Allocation Total", stats.dynamic_geometry.allocation_total_ms);
            DrawTimingStatLine("Upload Total", stats.dynamic_geometry.upload_total_ms);
            ImGui::Text("Allocations: %u", stats.dynamic_geometry.allocation_count);
            DrawByteStatLine("Vertex Bytes Requested", stats.dynamic_geometry.vertex_bytes_requested);
            DrawByteStatLine("Index Bytes Requested", stats.dynamic_geometry.index_bytes_requested);
            DrawUploadStatsTree("Vertex Uploads", stats.dynamic_geometry.vertex_upload);
            DrawUploadStatsTree("Index Uploads", stats.dynamic_geometry.index_upload);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Surfaces")) {
            DrawTimingStatLine("Begin Total", stats.surfaces.begin_total_ms);
            DrawTimingStatLine("Submit Total", stats.surfaces.submit_total_ms);
            DrawTimingStatLine("End Total", stats.surfaces.end_total_ms);
            DrawTimingStatLine("BeginSurface", stats.surfaces.begin_surface_ms);
            DrawTimingStatLine("EndSurface", stats.surfaces.end_surface_ms);
            DrawTimingStatLine("Global UBO Copy", stats.surfaces.global_ubo_copy_ms);
            DrawTimingStatLine("SetActiveVariant", stats.surfaces.set_active_variant_ms);
            DrawTimingStatLine("Sort", stats.surfaces.sort_ms);
            DrawTimingStatLine("Initial Shader Bind", stats.surfaces.initial_shader_bind_ms);
            DrawTimingStatLine("Apply Global Properties", stats.surfaces.global_properties_ms);
            DrawTimingStatLine("Pipeline Switches", stats.surfaces.pipeline_switch_ms);
            DrawTimingStatLine("Bind Custom BindGroup", stats.surfaces.custom_bind_group_ms);
            DrawTimingStatLine("Apply Local Properties", stats.surfaces.local_properties_ms);
            DrawTimingStatLine("DrawGeometry", stats.surfaces.draw_geometry_ms);
            DrawTimingStatLine("ResetActiveVariant", stats.surfaces.reset_active_variant_ms);
            ImGui::Text("Begin Count: %u", stats.surfaces.begin_count);
            ImGui::Text("Submit Count: %u", stats.surfaces.submit_count);
            ImGui::Text("End Count: %u", stats.surfaces.end_count);
            ImGui::Text("Draw Calls: %u", stats.surfaces.draw_call_count);
            ImGui::Text("Pipeline Binds: %u", stats.surfaces.pipeline_bind_count);
            ImGui::Text("Global Property Binds: %u", stats.surfaces.global_properties_count);
            ImGui::Text("Custom BindGroup Binds: %u", stats.surfaces.custom_bind_group_bind_count);
            ImGui::Text("Local Property Binds: %u", stats.surfaces.local_properties_count);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("SlugRenderText")) {
            DrawTimingStatLine("Total", stats.slug_text.total_ms);
            DrawTimingStatLine("Build Geometry", stats.slug_text.build_geometry_ms);
            DrawTimingStatLine("Allocate Dynamic Geometry", stats.slug_text.allocate_dynamic_geometry_ms);
            DrawTimingStatLine("Vertex MemCpy", stats.slug_text.vertex_memcpy_ms);
            DrawTimingStatLine("Index MemCpy", stats.slug_text.index_memcpy_ms);
            DrawTimingStatLine("Material Update", stats.slug_text.material_update_ms);
            ImGui::Text("Calls: %u", stats.slug_text.call_count);
            ImGui::Text("Quads: %u", stats.slug_text.quad_count);
            DrawByteStatLine("Input Text Bytes", stats.slug_text.text_byte_count);
            DrawByteStatLine("Generated Vertex Bytes", stats.slug_text.vertex_bytes_generated);
            DrawByteStatLine("Generated Index Bytes", stats.slug_text.index_bytes_generated);
            ImGui::TreePop();
        }

        DrawTimingStatLine("BeginMainRenderpass", stats.begin_main_renderpass_ms);
        DrawTimingStatLine("EndMainRenderpass", stats.end_main_renderpass_ms);
        ImGui::TreePop();
    }
}

int main(int argc, char** argv) {
    ThreadContext* thread_context = CreateThreadContext();
    SetCurrentThreadContext(thread_context);

    srand(time(NULL));
    ArenaAllocator* arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(64),
        .Alignment = KRAFT_SIZE_KB(64),
    });

    GameState* game_state = ArenaPush(arena, GameState);
    game_state->arena = arena;

    bool imgui_demo_window = true;
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
    auto window = CreateWindow({.Title = "TextDrawing", .Width = 1280, .Height = 768, .StartMaximized = true});
    auto _ = CreateRenderer({
        .Backend = RENDERER_BACKEND_TYPE_VULKAN,
        .MaterialBufferSize = 32,
    });

    // Window->Maximize();

    app_state.imgui_renderer.Init(game_state->arena);

    // Load the font
    const int size = 32;
    auto hack_regular = fs::ReadAllBytes(game_state->arena, String8Raw("res/fonts/Hack-Regular.ttf"));
    KASSERT(hack_regular.ptr);

    auto bangers = fs::ReadAllBytes(game_state->arena, String8Raw("res/fonts/Bangers-Regular.ttf"));
    KASSERT(bangers.ptr);

    auto noto_sans = fs::ReadAllBytes(game_state->arena, String8Raw("res/fonts/NotoSans-Regular.ttf"));
    KASSERT(noto_sans.ptr);

    r::FontAtlas font_atlas = r::LoadFontAtlas(arena, hack_regular.ptr, hack_regular.count, 32.0f);
    r::FontAtlas logo_font_atlas = r::LoadFontAtlas(arena, bangers.ptr, bangers.count, 64.0f);

    // Load slug font (vector-based text rendering)
    r::SlugFont slug_font = r::LoadSlugFont(arena, hack_regular.ptr, hack_regular.count);
    r::SlugFont noto_sans_font = r::LoadSlugFont(arena, noto_sans.ptr, noto_sans.count);

    auto slug_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/slug_font.kmt"));
    KASSERT(slug_material);

    auto bg_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/simple_2d.kmt"));
    auto bg_geometry = GeometrySystem::GetDefault2DGeometry();
    Mat4f bg_transform = ScaleMatrix(Vec3f{2560.0f, 1080.0f, 1.f}) * TranslationMatrix(Vec3f{0.f, 0.f, 0.f});
    r::Renderable background = {
        .ModelMatrix = bg_transform,
        .MaterialInstance = bg_material,
        .DrawData = bg_geometry->DrawData,
    };

    auto font_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/font.kmt"));
    KASSERT(font_material);

    auto selected_text_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/font.kmt"));
    KASSERT(selected_text_material);

    auto unselected_text_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/font.kmt"));
    KASSERT(unselected_text_material);

    auto logo_text_material = MaterialSystem::CreateMaterialFromFile(S("res/materials/font.kmt"));
    KASSERT(logo_text_material);

    MaterialSystem::SetTexture(font_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    MaterialSystem::SetProperty(font_material, String8Raw("DiffuseColor"), KRAFT_HEX(0xe74c3c));

    MaterialSystem::SetTexture(selected_text_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    MaterialSystem::SetProperty(selected_text_material, String8Raw("DiffuseColor"), KRAFT_HEX(0xecf0f1));

    MaterialSystem::SetTexture(unselected_text_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    MaterialSystem::SetProperty(unselected_text_material, String8Raw("DiffuseColor"), KRAFT_HEX(0x95a5a6));

    MaterialSystem::SetTexture(logo_text_material, String8Raw("DiffuseTexture"), logo_font_atlas.bitmap);
    MaterialSystem::SetProperty(logo_text_material, String8Raw("DiffuseColor"), KRAFT_HEX(0x9b59b6));

    auto bitmap_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/simple_2d.kmt"));
    MaterialSystem::SetTexture(bitmap_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    auto geometry = GeometrySystem::GetDefault2DGeometry();
    Mat4f transform = ScaleMatrix(Vec3f{font_atlas.width, font_atlas.height, 1.f}) * TranslationMatrix(Vec3f{1.f, 1.f, 0.f});
    r::Renderable font_bitmap;
    font_bitmap.EntityId = 1;
    font_bitmap.DrawData = geometry->DrawData;
    font_bitmap.MaterialInstance = bitmap_material;
    font_bitmap.ModelMatrix = transform;

    game_state->projection_matrix = OrthographicMatrix(-(f32)window->Width * 0.5f, (f32)window->Width * 0.5f, -(f32)window->Height * 0.5f, (f32)window->Height * 0.5f, -1.0f, 1.0f);

    Mat4f transformA = ScaleMatrix(Vec3f{size, -size, 1.f}) * TranslationMatrix(Vec3f{-size / 2.f, 0.f, 0.f});
    Mat4f transformB = ScaleMatrix(Vec3f{1, 1, 1.f}) * TranslationMatrix(Vec3f{0.0f, 0.0f, 0.f});

    // String8 text = String8Raw("/-/");
    String8 text = String8Raw("KICK  ");

    // Play Button
    Vec2f play_icon_path[] = {
        Vec2f(149.3021240234375f, 61.37730407714844f),     Vec2f(132.05206298828125f, 71.33622741699219f),    Vec2f(114.80194091796875f, 81.29515075683594f),
        Vec2f(97.5518798828125f, 91.25407409667969f),      Vec2f(80.3017578125f, 101.21299743652344f),        Vec2f(63.05169677734375f, 111.17192077636719f),
        Vec2f(45.8016357421875f, 121.13084411621094f),     Vec2f(28.551513671875f, 131.0897674560547f),       Vec2f(11.30145263671875f, 141.04869079589844f),
        Vec2f(-5.94866943359375f, 151.0076141357422f),     Vec2f(-23.19873046875f, 160.96653747558594f),      Vec2f(-40.4488525390625f, 170.9254608154297f),
        Vec2f(-57.69891357421875f, 180.88438415527344f),   Vec2f(-74.94900512695312f, 190.8433074951172f),    Vec2f(-92.99676513671875f, 199.14488220214844f),
        Vec2f(-112.61016845703125f, 202.20509338378906f),  Vec2f(-132.29498291015625f, 199.63084411621094f),  Vec2f(-150.51730346679688f, 191.74461364746094f),
        Vec2f(-165.9141845703125f, 179.2079315185547f),    Vec2f(-177.25341796875f, 162.9131622314453f),      Vec2f(-183.55130004882812f, 144.08750915527344f),
        Vec2f(-184.73785400390625f, 124.24037170410156f),  Vec2f(-184.73785400390625f, 104.32191467285156f),  Vec2f(-184.73785400390625f, 84.40339660644531f),
        Vec2f(-184.73785400390625f, 64.48490905761719f),   Vec2f(-184.73785400390625f, 44.56639099121094f),   Vec2f(-184.73785400390625f, 24.647903442382812f),
        Vec2f(-184.73785400390625f, 4.7294464111328125f),  Vec2f(-184.73785400390625f, -15.189163208007812f), Vec2f(-184.73785400390625f, -35.10774230957031f),
        Vec2f(-184.73785400390625f, -55.02601623535156f),  Vec2f(-184.73785400390625f, -74.94429016113281f),  Vec2f(-184.73785400390625f, -94.86274719238281f),
        Vec2f(-184.73785400390625f, -114.78120422363281f), Vec2f(-184.669189453125f, -134.6986846923828f),    Vec2f(-181.10775756835938f, -154.2306671142578f),
        Vec2f(-172.22210693359375f, -171.9815216064453f),  Vec2f(-158.73062133789062f, -186.54701232910156f), Vec2f(-141.77444458007812f, -196.8789825439453f),
        Vec2f(-122.6478271484375f, -202.20509338378906f),  Vec2f(-102.7982177734375f, -201.99891662597656f),  Vec2f(-83.8046875f, -196.2230987548828f),
        Vec2f(-66.41583251953125f, -186.5222930908203f),   Vec2f(-49.16583251953125f, -176.56336975097656f),  Vec2f(-31.915283203125f, -166.6042022705078f),
        Vec2f(-14.665374755859375f, -156.64540100097656f), Vec2f(2.5845947265625f, -146.68653869628906f),     Vec2f(19.834716796875f, -136.7276153564453f),
        Vec2f(37.08489990234375f, -126.76856994628906f),   Vec2f(54.3348388671875f, -116.80976867675781f),    Vec2f(71.5849609375f, -106.85078430175781f),
        Vec2f(88.8349609375f, -96.89192199707031f),        Vec2f(106.0850830078125f, -86.93299865722656f),    Vec2f(123.3350830078125f, -76.97407531738281f),
        Vec2f(140.585205078125f, -67.01515197753906f),     Vec2f(157.4844970703125f, -56.50636291503906f),    Vec2f(171.347412109375f, -42.29676818847656f),
        Vec2f(180.6533203125f, -24.761734008789062f),      Vec2f(184.73785400390625f, -5.3315582275390625f),  Vec2f(183.390869140625f, 14.478775024414062f),
        Vec2f(176.67462158203125f, 33.16151428222656f),    Vec2f(164.9949951171875f, 49.21241760253906f),
    };

    // Controller
    Vec2f controller_icon_path[] = {
        Vec2f(-3.9471282958984375f, 144.5929718017578f),   Vec2f(-26.555191040039062f, 144.5696563720703f),   Vec2f(-49.14497375488281f, 143.66944885253906f),
        Vec2f(-71.67173767089844f, 141.7644500732422f),    Vec2f(-94.06938171386719f, 138.69947814941406f),   Vec2f(-116.23936462402344f, 134.2847137451172f),
        Vec2f(-138.03306579589844f, 128.28855895996094f),  Vec2f(-159.22474670410156f, 120.43293762207031f),  Vec2f(-179.4700469970703f, 110.39637756347656f),
        Vec2f(-198.2459259033203f, 97.83570861816406f),    Vec2f(-214.7696990966797f, 82.44764709472656f),    Vec2f(-227.8903045654297f, 64.09223937988281f),
        Vec2f(-237.70008850097656f, 43.73704528808594f),   Vec2f(-245.32789611816406f, 22.462112426757812f),  Vec2f(-250.9916534423828f, 0.5815582275390625f),
        Vec2f(-254.61341857910156f, -21.726943969726562f), Vec2f(-255.90760803222656f, -44.28712463378906f),  Vec2f(-254.3777313232422f, -66.82624816894531f),
        Vec2f(-249.30979919433594f, -88.82911682128906f),  Vec2f(-239.8968963623047f, -109.33149719238281f),  Vec2f(-225.7198944091797f, -126.85774230957031f),
        Vec2f(-207.3616485595703f, -139.9217071533203f),   Vec2f(-185.45668029785156f, -144.67457580566406f), Vec2f(-163.30287170410156f, -140.6908721923828f),
        Vec2f(-143.02757263183594f, -130.8028106689453f),  Vec2f(-124.44053649902344f, -117.93782043457031f), Vec2f(-104.91752624511719f, -106.55250549316406f),
        Vec2f(-84.36714172363281f, -97.14845275878906f),   Vec2f(-62.97068786621094f, -89.87336730957031f),   Vec2f(-40.93455505371094f, -84.86134338378906f),
        Vec2f(-18.491043090820312f, -82.22126770019531f),  Vec2f(4.1096954345703125f, -81.80702209472656f),   Vec2f(26.682144165039062f, -82.88655090332031f),
        Vec2f(49.00831604003906f, -86.38551330566406f),    Vec2f(70.84034729003906f, -92.22236633300781f),    Vec2f(91.95619201660156f, -100.27662658691406f),
        Vec2f(112.15901184082031f, -110.40602111816406f),  Vec2f(131.27821350097656f, -122.45790100097656f),  Vec2f(150.14271545410156f, -134.8781280517578f),
        Vec2f(171.20033264160156f, -142.93238830566406f),  Vec2f(193.6479034423828f, -144.1179962158203f),    Vec2f(214.48133850097656f, -135.86854553222656f),
        Vec2f(231.46925354003906f, -121.05268859863281f),  Vec2f(243.96192932128906f, -102.28260803222656f),  Vec2f(251.74537658691406f, -81.10023498535156f),
        Vec2f(255.45838928222656f, -58.82368469238281f),   Vec2f(255.90760803222656f, -36.23439025878906f),   Vec2f(253.73365783691406f, -13.740615844726562f),
        Vec2f(249.3565216064453f, 8.433303833007812f),     Vec2f(242.9910430908203f, 30.120437622070312f),    Vec2f(234.6294708251953f, 51.11668395996094f),
        Vec2f(223.8443145751953f, 70.96278381347656f),     Vec2f(209.3585968017578f, 88.26936340332031f),     Vec2f(191.9256134033203f, 102.62596130371094f),
        Vec2f(172.55372619628906f, 114.25148010253906f),   Vec2f(151.92298889160156f, 123.47343444824219f),   Vec2f(130.48524475097656f, 130.63194274902344f),
        Vec2f(108.53669738769531f, 136.0349578857422f),    Vec2f(86.27162170410156f, 139.94346618652344f),    Vec2f(63.81880187988281f, 142.5738067626953f),
        Vec2f(41.26289367675781f, 144.10255432128906f),    Vec2f(18.662063598632812f, 144.67457580566406f),
    };

    struct PointsArray {
        Vec2f* points;
        u32 num_points;
        GeometryData geometry_data;
        Geometry* geometry;
    };

    PointsArray points1_array = {
        .points = play_icon_path,
        .num_points = KRAFT_C_ARRAY_SIZE(play_icon_path),
    };

    PointsArray points2_array = {
        .points = controller_icon_path,
        .num_points = KRAFT_C_ARRAY_SIZE(controller_icon_path),
    };

    u32 max_points = math::Max(KRAFT_C_ARRAY_SIZE(play_icon_path), KRAFT_C_ARRAY_SIZE(controller_icon_path));
    r::Vertex2D* vertices = ArenaPushArray(arena, r::Vertex2D, 4 * max_points);
    u32* indices = ArenaPushArray(arena, u32, 6 * max_points);

    PointsArray* active_points_array = &points1_array;
    Vec4f color = KRAFT_HEX(0xe74c3c);

    struct CharacterState {
        Vec2f current_position;
        u32 current_point_index;
        f32 progress;
    } character_states[1024];

    for (u32 i = 0; i < max_points; i++) {
        char character = text.ptr[i % text.count];

        u32 char_index = character - font_atlas.start_character;
        r::FontGlyph glyph = font_atlas.glyphs[char_index];
        // stbtt_packedchar   packed_char = packed_characters[char_index];
        // stbtt_aligned_quad aligned_quad = aligned_quads[char_index];

        float offset_x = active_points_array->points[i].x;
        float offset_y = active_points_array->points[i].y;
        character_states[i] = {{offset_x, offset_y}, i, 0.0f};

        // top-right
        vertices[4 * i + 0] = r::Vertex2D{
            .Position =
                vec2{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_0,
                },
            .UV = {glyph.u1, glyph.v0},
            .Color = color,
        };

        // bottom-right
        vertices[4 * i + 1] = r::Vertex2D{
            .Position =
                vec2{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_1,
                },
            .UV = {glyph.u1, glyph.v1},
            .Color = color,
        };

        // bottom-left
        vertices[4 * i + 2] = r::Vertex2D{
            .Position =
                vec2{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_1,
                },
            .UV = {glyph.u0, glyph.v1},
            .Color = color,
        };

        // top-left
        vertices[4 * i + 3] = r::Vertex2D{
            .Position =
                vec2{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_0,
                },
            .UV = {glyph.u0, glyph.v0},
            .Color = color,
        };

        indices[6 * i + 0] = 4 * i + 0;
        indices[6 * i + 1] = 4 * i + 1;
        indices[6 * i + 2] = 4 * i + 2;
        indices[6 * i + 3] = 4 * i + 2;
        indices[6 * i + 4] = 4 * i + 3;
        indices[6 * i + 5] = 4 * i + 0;
    }

    points1_array.geometry_data = GeometryData{
        .VertexCount = 4 * points1_array.num_points,
        .IndexCount = 6 * points1_array.num_points,
        .VertexSize = sizeof(r::Vertex2D),
        .IndexSize = sizeof(u32),
        .Vertices = vertices,
        .Indices = indices,
    };
    points1_array.geometry = GeometrySystem::AcquireGeometryWithData(points1_array.geometry_data);
    KASSERT(points1_array.geometry);

    points2_array.geometry_data = GeometryData{
        .VertexCount = 4 * points2_array.num_points,
        .IndexCount = 6 * points2_array.num_points,
        .VertexSize = sizeof(r::Vertex2D),
        .IndexSize = sizeof(u32),
        .Vertices = vertices,
        .Indices = indices,
    };
    points2_array.geometry = GeometrySystem::AcquireGeometryWithData(points2_array.geometry_data);
    KASSERT(points2_array.geometry);

    MaterialSystem::SetProperty(font_material, String8Raw("DiffuseColor"), color);

    // Store original positions and phase offsets for each character
    // r::Vertex2D* original_vertices = ArenaPushArray(arena, r::Vertex2D, 4 * text.count);
    // MemCpy(original_vertices, active_points_array->vertices, sizeof(r::Vertex2D) * 4 * text.count);

    // f32* phase_offsets = ArenaPushArray(arena, f32, text.count);
    // for (u32 i = 0; i < text.count; i++)
    // {
    //     phase_offsets[i] = ((f32)rand() / (f32)RAND_MAX) * 6.28318f; // 2*PI
    // }

    // f32 wave_amplitude = 3.0f;
    // f32 wave_speed = 8.0f;
    f32 animation_speed = 1.0f;

    auto play_button_text = r::RenderText(
        selected_text_material,
        &font_atlas,
        S("Play"),
        TranslationMatrix(Vec3f{
            -165.0f,
            -300.0f,
            0.0f,
        }),
        KRAFT_HEX(0xecf0f1)
    );

    auto settings_button_text = r::RenderText(
        unselected_text_material,
        &font_atlas,
        S("Settings"),
        TranslationMatrix(Vec3f{
            45.0f,
            -300.0f,
            0.0f,
        }),
        KRAFT_HEX(0x95a5a6)
    );

    Vec4f logo_color = KRAFT_HEX(0xf1c40f);
    Transform logo_transform = Transform{
        .position = {-100.0f, 0.0, 0.0f},
        .scale = {1.0f, 1.0f},
    };
    auto logo_text = r::RenderText(logo_text_material, &logo_font_atlas, S("sidekicks"), MatrixFromTransform(logo_transform), logo_color);

    bool play_selected = true;

    srand(time(NULL));
    while (Engine::running) {
        static i32 seed = 0;
        BeginAppStatsFrame(&app_state.stats);
        AppFrameStats& app_frame_stats = app_state.stats.current_frame;
        KRAFT_TIMING_STATS_BEGIN(app_frame_total_timer);

        KRAFT_TIMING_STATS_BEGIN(scratch_begin_timer);
        TempArena scratch = ScratchBegin(&game_state->arena, 1);
        KRAFT_TIMING_STATS_ACCUM(scratch_begin_timer, app_frame_stats.scratch_begin_ms);
        app_state.frame_count++;

        f64 frame_start_time = Platform::GetAbsoluteTime();
        f64 delta_time = frame_start_time - app_state.last_time;
        Time::delta_time = delta_time;
        app_state.time_since_last_frame += delta_time;
        app_state.last_time = frame_start_time;
        app_frame_stats.delta_time_ms = delta_time * 1000.0;

        // Poll events
        KRAFT_TIMING_STATS_BEGIN(engine_tick_timer);
        Engine::Tick();
        KRAFT_TIMING_STATS_ACCUM(engine_tick_timer, app_frame_stats.engine_tick_ms);
        const EngineTickStats& engine_tick_stats = Engine::GetStats().current_tick;
        app_frame_stats.engine_time_update_ms = engine_tick_stats.time_update_ms;
        app_frame_stats.engine_input_update_ms = engine_tick_stats.input_update_ms;
        app_frame_stats.engine_poll_events_ms = engine_tick_stats.poll_events_ms;
        app_frame_stats.engine_suspended = Engine::suspended;

        // We only want to render when the engine is not suspended
        if (!Engine::suspended) {
            app_frame_stats.rendered_frame = true;
            KRAFT_TIMING_STATS_BEGIN(render_block_timer);

            KRAFT_TIMING_STATS_BEGIN(renderer_prepare_frame_timer);
            g_Renderer->PrepareFrame();
            KRAFT_TIMING_STATS_ACCUM(renderer_prepare_frame_timer, app_frame_stats.renderer_prepare_frame_ms);

            KRAFT_TIMING_STATS_BEGIN(renderer_begin_main_renderpass_timer);
            g_Renderer->BeginMainRenderpass();
            KRAFT_TIMING_STATS_ACCUM(renderer_begin_main_renderpass_timer, app_frame_stats.renderer_begin_main_renderpass_ms);

            KRAFT_TIMING_STATS_BEGIN(scene_build_timer);
            r::RenderSurface* main_surface = g_Renderer->GetMainSurface();
            r::GlobalShaderData global_data = {};
            global_data.ViewProjection = mat4(Identity) * game_state->projection_matrix;
            global_data.CameraPosition.r = Time::elapsed_time;
            KRAFT_TIMING_STATS_BEGIN(scene_surface_begin_timer);
            main_surface->Begin(scratch.arena, global_data);
            KRAFT_TIMING_STATS_ACCUM(scene_surface_begin_timer, app_frame_stats.scene_surface_begin_ms);

            KRAFT_TIMING_STATS_BEGIN(scene_non_slug_update_timer);
            // f32 y = Sin(Time::elapsed_time);
            // KDEBUG("y = %f", y);
            transformA = ScaleMatrix(Vec3f{size, -size, 1.f}) * TranslationMatrix(Vec3f{-size / 2.f, Sin(Time::elapsed_time * 2.0f) * 100.0f, 0.f});
            // transformB = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ size / 2.f, Sin(Time::elapsed_time * 2.f + 100.0f) * 100.0f, 0.f });
            // transformB = ScaleMatrix(Vec3f{ 1.0f, 1.f, 1.f });

#if 0
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
#else
            f32 progress = Time::delta_time * animation_speed;
            for (u32 i = 0; i < active_points_array->num_points; i++) {
                char character = text.ptr[i % text.count];

                u32 char_index = character - font_atlas.start_character;
                r::FontGlyph glyph = font_atlas.glyphs[char_index];
                // stbtt_packedchar packed_char = packed_characters[char_index];

                // CharacterState character_state = character_states[i];
                character_states[i].progress += progress;
                if (character_states[i].progress >= 1.0f) {
                    character_states[i].current_point_index = (character_states[i].current_point_index + 1) % active_points_array->num_points;
                    Vec2f point = active_points_array->points[character_states[i].current_point_index];
                    character_states[i].current_position = {point.x, point.y};
                    character_states[i].progress = 0.0f;
                }

                u32 next_point_index = (character_states[i].current_point_index + 1) % active_points_array->num_points;
                // Vec2f current_point = active_points_array->points[character_states[i].current_point_index];
                Vec2f next_point = active_points_array->points[next_point_index];
                f32 offset_x = lerp(character_states[i].current_position.x, next_point.x, character_states[i].progress);
                f32 offset_y = lerp(character_states[i].current_position.y, next_point.y, character_states[i].progress);

                vertices[4 * i + 0].Position = vec2{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_0,
                };
                vertices[4 * i + 1].Position = vec2{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_1,
                };
                vertices[4 * i + 2].Position = vec2{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_1,
                };
                vertices[4 * i + 3].Position = vec2{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_0,
                };
            }

            // We are not updating the indices, so lets unset that
            active_points_array->geometry_data.Indices = nullptr;
            GeometrySystem::UpdateGeometry(active_points_array->geometry, active_points_array->geometry_data);
#endif

            if ((!InputSystem::IsKeyDown(Keys::KEY_RIGHT) && InputSystem::WasKeyDown(Keys::KEY_RIGHT)) || (!InputSystem::IsKeyDown(Keys::KEY_LEFT) && InputSystem::WasKeyDown(Keys::KEY_LEFT))) {
                if (play_selected) {
                    play_selected = false;
                    play_button_text.MaterialInstance = unselected_text_material;
                    settings_button_text.MaterialInstance = selected_text_material;
                    active_points_array = &points2_array;
                } else {
                    play_selected = true;
                    play_button_text.MaterialInstance = selected_text_material;
                    settings_button_text.MaterialInstance = unselected_text_material;
                    active_points_array = &points1_array;
                }

                for (u32 i = 0; i < KRAFT_C_ARRAY_SIZE(character_states); i++) {
                    character_states[i].progress = 0.1f;
                }
            }
            KRAFT_TIMING_STATS_ACCUM(scene_non_slug_update_timer, app_frame_stats.scene_non_slug_update_ms);

            KRAFT_TIMING_STATS_BEGIN(scene_non_slug_submit_timer);
            main_surface->Submit(background);
            main_surface->Submit({
                .ModelMatrix = transformB,
                .MaterialInstance = font_material,
                .DrawData = active_points_array->geometry->DrawData,
                .EntityId = 420,
            });

            main_surface->Submit(play_button_text);
            main_surface->Submit(settings_button_text);
            main_surface->Submit(logo_text);
            KRAFT_TIMING_STATS_ACCUM(scene_non_slug_submit_timer, app_frame_stats.scene_non_slug_submit_ms);

            // Slug text (vector-based rendering with custom bind group)
            static i32 rows = 26;
            static i32 cols = 80;
            static f32 slug_font_size = 16.0f;
            static vec2 slug_position = {-window->Width / 2.0f, -window->Height / 2.0f};

            {

                KRAFT_TIMING_STATS_BEGIN(scene_static_slug_total_timer);
                for (i32 x = 0; x < rows; x++) {
                    for (i32 y = 0; y < cols; y++) {
                        u32 x_padding = x * 100.0f;
                        u32 y_padding = y * (slug_font_size + 4.0f);
                        auto transform = TranslationMatrix(Vec3f{slug_position.x + x_padding, slug_position.y + y_padding, 0.0f});
                        KRAFT_TIMING_STATS_BEGIN(slug_render_text_timer);
                        auto slug_text = r::SlugRenderText(slug_material, &noto_sans_font, GetStaticRandomText(scratch.arena, 11, seed + x + y), transform, KRAFT_HEX(0x2ecc71), slug_font_size);
                        KRAFT_TIMING_STATS_ACCUM(slug_render_text_timer, app_frame_stats.scene_static_slug_render_text_ms);
                        KRAFT_TIMING_STATS_BEGIN(slug_submit_timer);
                        main_surface->Submit(slug_text);
                        KRAFT_TIMING_STATS_ACCUM(slug_submit_timer, app_frame_stats.scene_static_slug_submit_ms);
                    }
                }
                KRAFT_TIMING_STATS_ACCUM(scene_static_slug_total_timer, app_frame_stats.scene_static_slug_total_ms);
                static u64 raw_log_counter = 0;
                if (++raw_log_counter % 120 == 0) {
                    fflush(stdout);
                }

                // Dynamic slug text - changes every frame
                KRAFT_TIMING_STATS_BEGIN(scene_dynamic_slug_total_timer);
                int frame_val = (int)(Time::elapsed_time * 2.0f);
                const char* phrases[] = {"Frame ", "Time: ", "Slug! ", "BDA!! ", "Hello ", "Kraft "};
                int phrase_idx = frame_val % KRAFT_C_ARRAY_SIZE(phrases);

                // Format a string with the frame counter
                String8 dynamic_str = StringFormat(scratch.arena, "%s%d", phrases[phrase_idx], (int)(Time::elapsed_time * 10.0f));

                KRAFT_TIMING_STATS_BEGIN(scene_dynamic_slug_render_text_timer);
                auto slug_dynamic1 = r::SlugRenderText(slug_material, &slug_font, dynamic_str, TranslationMatrix(Vec3f{-300.0f, 100.0f, 0.0f}), KRAFT_HEX(0xe74c3c), 48.0f);
                KRAFT_TIMING_STATS_ACCUM(scene_dynamic_slug_render_text_timer, app_frame_stats.scene_dynamic_slug_render_text_ms);
                if (slug_dynamic1.DrawData.IndexCount > 0) {
                    KRAFT_TIMING_STATS_BEGIN(scene_dynamic_slug_submit_timer);
                    main_surface->Submit(slug_dynamic1);
                    KRAFT_TIMING_STATS_ACCUM(scene_dynamic_slug_submit_timer, app_frame_stats.scene_dynamic_slug_submit_ms);
                }

                // Second dynamic text - bouncing with sin wave
                f32 bounce_y = Sin(Time::elapsed_time * 3.0f) * 50.0f;
                String8 dynamic_str2 = StringFormat(scratch.arena, "FPS: %d", app_state.frame_count);

                KRAFT_TIMING_STATS_BEGIN(scene_dynamic_slug_render_text_timer2);
                auto slug_dynamic2 = r::SlugRenderText(slug_material, &slug_font, dynamic_str2, TranslationMatrix(Vec3f{-300.0f, -100.0f + bounce_y, 0.0f}), KRAFT_HEX(0x3498db), 40.0f);
                KRAFT_TIMING_STATS_ACCUM(scene_dynamic_slug_render_text_timer2, app_frame_stats.scene_dynamic_slug_render_text_ms);
                if (slug_dynamic2.DrawData.IndexCount > 0) {
                    KRAFT_TIMING_STATS_BEGIN(scene_dynamic_slug_submit_timer2);
                    main_surface->Submit(slug_dynamic2);
                    KRAFT_TIMING_STATS_ACCUM(scene_dynamic_slug_submit_timer2, app_frame_stats.scene_dynamic_slug_submit_ms);
                }
                KRAFT_TIMING_STATS_ACCUM(scene_dynamic_slug_total_timer, app_frame_stats.scene_dynamic_slug_total_ms);
            }

            KRAFT_TIMING_STATS_BEGIN(scene_surface_end_timer);
            main_surface->End();
            KRAFT_TIMING_STATS_ACCUM(scene_surface_end_timer, app_frame_stats.scene_surface_end_ms);
            KRAFT_TIMING_STATS_ACCUM(scene_build_timer, app_frame_stats.scene_build_ms);

            // Draw ImGui
            KRAFT_TIMING_STATS_BEGIN(imgui_total_timer);

            KRAFT_TIMING_STATS_BEGIN(imgui_begin_frame_timer);
            app_state.imgui_renderer.BeginFrame();
            KRAFT_TIMING_STATS_ACCUM(imgui_begin_frame_timer, app_frame_stats.imgui_begin_frame_ms);

            KRAFT_TIMING_STATS_BEGIN(imgui_build_ui_timer);
            ImGui::ShowDemoWindow(&imgui_demo_window);
            DrawGameUI(game_state);

            ImGui::Begin("TextRenderDebug");
            // ImGui::DragFloat("Wave amplitude", &wave_amplitude);
            // ImGui::DragFloat("Wave speed", &wave_speed);
            ImGui::DragFloat("Animation Speed", &animation_speed);

            static Vec2f scale = {1.0f, 1.0f};
            static Vec2f position = {-270.0f, -270.0f};
            if (ImGui::DragFloat2("Position", position._data)) {
                transformB = ScaleMatrix(Vec3f{scale.x, scale.y, 1.f}) * TranslationMatrix(Vec3f{position.x, position.y, 0.f});
            }

            if (ImGui::DragFloat2("Scale", scale._data)) {
                transformB = ScaleMatrix(Vec3f{scale.x, scale.y, 1.f}) * TranslationMatrix(Vec3f{position.x, position.y, 0.f});
            }

            if (ImGui::Button("Switch Path")) {
                if (active_points_array == &points1_array) {
                    active_points_array = &points2_array;
                } else {
                    active_points_array = &points1_array;
                }

                for (u32 i = 0; i < KRAFT_C_ARRAY_SIZE(character_states); i++) {
                    // character_states[i].current_point_index = i;
                    character_states[i].progress = 0.1f;
                }
            }

            if (ImGui::ColorEdit4("Logo Color", logo_color._data)) {
                MaterialSystem::SetProperty(logo_text_material, S("DiffuseColor"), logo_color);
            }

            if (ImGui::DragFloat2("Logo Position", logo_transform.position._data)) {
                logo_text.ModelMatrix = MatrixFromTransform(logo_transform);
            }

            if (ImGui::DragFloat2("Logo Scale", logo_transform.scale._data)) {
                logo_text.ModelMatrix = MatrixFromTransform(logo_transform);
            }

            if (ImGui::DragFloat2("Slug Matrix Position", slug_position._data)) {}
            if (ImGui::DragInt("Slug Matrix Rows", &rows)) {}
            if (ImGui::DragInt("Slug Matrix Cols", &cols)) {}

            ImGui::End();

            ImGui::Begin("Render Stats");
            DrawAppFrameStatsTree("App Current Frame", app_state.stats.current_frame);
            DrawAppFrameStatsTree("App Last Completed Frame", app_state.stats.last_completed_frame);
            const r::RendererStats& render_stats = g_Renderer->GetStats();
            DrawRendererFrameStatsTree("Renderer Current Frame", render_stats.current_frame);
            DrawRendererFrameStatsTree("Renderer Last Completed Frame", render_stats.last_completed_frame);
            ImGui::End();
            KRAFT_TIMING_STATS_ACCUM(imgui_build_ui_timer, app_frame_stats.imgui_build_ui_ms);

            KRAFT_TIMING_STATS_BEGIN(imgui_end_frame_timer);
            app_state.imgui_renderer.EndFrame();
            KRAFT_TIMING_STATS_ACCUM(imgui_end_frame_timer, app_frame_stats.imgui_end_frame_ms);
            KRAFT_TIMING_STATS_ACCUM(imgui_total_timer, app_frame_stats.imgui_total_ms);

            KRAFT_TIMING_STATS_BEGIN(renderer_end_main_renderpass_timer);
            g_Renderer->EndMainRenderpass();
            KRAFT_TIMING_STATS_ACCUM(renderer_end_main_renderpass_timer, app_frame_stats.renderer_end_main_renderpass_ms);

            KRAFT_TIMING_STATS_BEGIN(imgui_platform_windows_timer);
            app_state.imgui_renderer.EndFrameUpdatePlatformWindows();
            KRAFT_TIMING_STATS_ACCUM(imgui_platform_windows_timer, app_frame_stats.imgui_platform_windows_ms);

            KRAFT_TIMING_STATS_ACCUM(render_block_timer, app_frame_stats.render_block_ms);
        }

        if (app_state.time_since_last_frame >= 1.f) {
            KRAFT_TIMING_STATS_BEGIN(title_update_timer);
            String8 window_title = StringFormat(scratch.arena, "TextDrawing | FrameTime: %.2f ms | %d fps", delta_time * 1000.f, app_state.frame_count);
            window->SetWindowTitle(window_title.str);

            app_state.time_since_last_frame = 0.f;
            app_state.frame_count = 0;
            KRAFT_TIMING_STATS_ACCUM(title_update_timer, app_frame_stats.title_update_ms);

            seed = rand();
        }

        KRAFT_TIMING_STATS_BEGIN(scratch_end_timer);
        ScratchEnd(scratch);
        KRAFT_TIMING_STATS_ACCUM(scratch_end_timer, app_frame_stats.scratch_end_ms);
        KRAFT_TIMING_STATS_SET(app_frame_total_timer, app_frame_stats.total_ms);
    }

    // DestroyRenderer(Renderer);
    // DestroyWindow(Window);
    DestroyEngine();

    return 0;
}
