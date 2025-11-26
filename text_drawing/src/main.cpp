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

#include <renderer/kraft_renderer_includes.h>
#include <renderer/kraft_renderer_includes.cpp>

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

f32 lerp(f32 a, f32 b, f32 amount)
{
    return a + (b - a) * amount;
};

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

    r::FontAtlas font_atlas = r::LoadFontAtlas(arena, hack_regular.ptr, hack_regular.count, 32.0f);

    auto          bg_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/simple_2d.kmt"), r::Handle<r::RenderPass>::Invalid());
    auto          bg_geometry = GeometrySystem::GetDefault2DGeometry();
    Mat4f         bg_transform = ScaleMatrix(Vec3f{ 2560.0f, 1080.0f, 1.f }) * TranslationMatrix(Vec3f{ 0.f, 0.f, 0.f });
    r::Renderable background = {
        .ModelMatrix = bg_transform,
        .MaterialInstance = bg_material,
        .GeometryId = bg_geometry->InternalID,
    };

    auto font_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/font.kmt"), r::Handle<r::RenderPass>::Invalid());
    auto selected_text_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/font.kmt"), r::Handle<r::RenderPass>::Invalid());
    auto unselected_text_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/font.kmt"), r::Handle<r::RenderPass>::Invalid());
    KASSERT(font_material);

    MaterialSystem::SetTexture(font_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    MaterialSystem::SetProperty(font_material, String8Raw("DiffuseColor"), KRAFT_HEX(0xe74c3c));

    MaterialSystem::SetTexture(selected_text_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    MaterialSystem::SetProperty(selected_text_material, String8Raw("DiffuseColor"), KRAFT_HEX(0xecf0f1));

    MaterialSystem::SetTexture(unselected_text_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    MaterialSystem::SetProperty(unselected_text_material, String8Raw("DiffuseColor"), KRAFT_HEX(0x95a5a6));

    auto bitmap_material = MaterialSystem::CreateMaterialFromFile(String8Raw("res/materials/simple_2d.kmt"), r::Handle<r::RenderPass>::Invalid());
    MaterialSystem::SetTexture(bitmap_material, String8Raw("DiffuseTexture"), font_atlas.bitmap);
    auto          geometry = GeometrySystem::GetDefault2DGeometry();
    Mat4f         transform = ScaleMatrix(Vec3f{ font_atlas.width, font_atlas.height, 1.f }) * TranslationMatrix(Vec3f{ 1.f, 1.f, 0.f });
    r::Renderable font_bitmap;
    font_bitmap.EntityId = 1;
    font_bitmap.GeometryId = geometry->InternalID;
    font_bitmap.MaterialInstance = bitmap_material;
    font_bitmap.ModelMatrix = transform;

    r::GlobalShaderData global_data = {};
    game_state->projection_matrix = OrthographicMatrix(-(f32)window->Width * 0.5f, (f32)window->Width * 0.5f, -(f32)window->Height * 0.5f, (f32)window->Height * 0.5f, -1.0f, 1.0f);
    // game_state->projection_matrix = PerspectiveMatrix(DegToRadians(45.0f), (float)Window->Width / (float)Window->Height, 0.1f, 1000.f);
    global_data.View = Mat4f(Identity);

    Mat4f transformA = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ -size / 2.f, 0.f, 0.f });
    Mat4f transformB = ScaleMatrix(Vec3f{ 1, 1, 1.f }) * TranslationMatrix(Vec3f{ 0.0f, 0.0f, 0.f });

    String8 text = String8Raw("SIDEKICKS    ");

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

    struct PointsArray
    {
        Vec2f*       points;
        u32          num_points;
        GeometryData geometry_data;
        Geometry*    geometry;
    };

    PointsArray points1_array = {
        .points = play_icon_path,
        .num_points = KRAFT_C_ARRAY_SIZE(play_icon_path),
    };

    PointsArray points2_array = {
        .points = controller_icon_path,
        .num_points = KRAFT_C_ARRAY_SIZE(controller_icon_path),
    };

    u32          max_points = math::Max(KRAFT_C_ARRAY_SIZE(play_icon_path), KRAFT_C_ARRAY_SIZE(controller_icon_path));
    r::Vertex2D* vertices = ArenaPushArray(arena, r::Vertex2D, 4 * max_points);
    u32*         indices = ArenaPushArray(arena, u32, 6 * max_points);

    PointsArray* active_points_array = &points1_array;
    Vec4f        color = KRAFT_HEX(0xe74c3c);

    struct CharacterState
    {
        Vec2f current_position;
        u32   current_point_index;
        f32   progress;
    } character_states[1024];

    for (u32 i = 0; i < max_points; i++)
    {
        char character = text.ptr[i % text.count];

        u32          char_index = character - font_atlas.start_character;
        r::FontGlyph glyph = font_atlas.glyphs[char_index];
        // stbtt_packedchar   packed_char = packed_characters[char_index];
        // stbtt_aligned_quad aligned_quad = aligned_quads[char_index];

        float offset_x = active_points_array->points[i].x;
        float offset_y = active_points_array->points[i].y;
        character_states[i] = { { offset_x, offset_y }, i, 0.0f };

        // top-right
        vertices[4 * i + 0] = r::Vertex2D{
            .Position =
                Vec3f{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_0,
                    0.f,
                },
            .UV = { glyph.u1, glyph.v0 },
            .Color = color,
        };

        // bottom-right
        vertices[4 * i + 1] = r::Vertex2D{
            .Position =
                Vec3f{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_1,
                    0.f,
                },
            .UV = { glyph.u1, glyph.v1 },
            .Color = color,
        };

        // bottom-left
        vertices[4 * i + 2] = r::Vertex2D{
            .Position =
                Vec3f{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_1,
                    0.f,
                },
            .UV = { glyph.u0, glyph.v1 },
            .Color = color,
        };

        // top-left
        vertices[4 * i + 3] = r::Vertex2D{
            .Position =
                Vec3f{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_0,
                    0.f,
                },
            .UV = { glyph.u0, glyph.v0 },
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

    bool play_selected = true;

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
            global_data.CameraPosition.r = Time::elapsed_time;

            // f32 y = Sin(Time::elapsed_time);
            // KDEBUG("y = %f", y);
            transformA = ScaleMatrix(Vec3f{ size, -size, 1.f }) * TranslationMatrix(Vec3f{ -size / 2.f, Sin(Time::elapsed_time * 2.0f) * 100.0f, 0.f });
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
            for (u32 i = 0; i < active_points_array->num_points; i++)
            {
                char character = text.ptr[i % text.count];

                u32          char_index = character - font_atlas.start_character;
                r::FontGlyph glyph = font_atlas.glyphs[char_index];
                // stbtt_packedchar packed_char = packed_characters[char_index];

                // CharacterState character_state = character_states[i];
                character_states[i].progress += progress;
                if (character_states[i].progress >= 1.0f)
                {
                    character_states[i].current_point_index = (character_states[i].current_point_index + 1) % active_points_array->num_points;
                    Vec2f point = active_points_array->points[character_states[i].current_point_index];
                    character_states[i].current_position = { point.x, point.y };
                    character_states[i].progress = 0.0f;
                }

                u32 next_point_index = (character_states[i].current_point_index + 1) % active_points_array->num_points;
                // Vec2f current_point = active_points_array->points[character_states[i].current_point_index];
                Vec2f next_point = active_points_array->points[next_point_index];
                f32   offset_x = lerp(character_states[i].current_position.x, next_point.x, character_states[i].progress);
                f32   offset_y = lerp(character_states[i].current_position.y, next_point.y, character_states[i].progress);

                vertices[4 * i + 0].Position = Vec3f{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_0,
                    0.f,
                };
                vertices[4 * i + 1].Position = Vec3f{
                    offset_x + glyph.baseline_xoff_1,
                    offset_y - glyph.baseline_yoff_1,
                    0.f,
                };
                vertices[4 * i + 2].Position = Vec3f{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_1,
                    0.f,
                };
                vertices[4 * i + 3].Position = Vec3f{
                    offset_x + glyph.baseline_xoff_0,
                    offset_y - glyph.baseline_yoff_0,
                    0.f,
                };
            }

            // We are not updating the indices, so lets unset that
            active_points_array->geometry_data.Indices = nullptr;
            GeometrySystem::UpdateGeometry(active_points_array->geometry, active_points_array->geometry_data);
#endif

            if ((!InputSystem::IsKeyDown(Keys::KEY_RIGHT) && InputSystem::WasKeyDown(Keys::KEY_RIGHT)) || (!InputSystem::IsKeyDown(Keys::KEY_LEFT) && InputSystem::WasKeyDown(Keys::KEY_LEFT)))
            {
                if (play_selected)
                {
                    play_selected = false;
                    play_button_text.MaterialInstance = unselected_text_material;
                    settings_button_text.MaterialInstance = selected_text_material;
                    active_points_array = &points2_array;
                }
                else
                {
                    play_selected = true;
                    play_button_text.MaterialInstance = selected_text_material;
                    settings_button_text.MaterialInstance = unselected_text_material;
                    active_points_array = &points1_array;
                }

                for (u32 i = 0; i < KRAFT_C_ARRAY_SIZE(character_states); i++)
                {
                    character_states[i].progress = 0.1f;
                }
            }

            // g_Renderer->AddRenderable({ .EntityId = 1, .GeometryId = geometry->InternalID, .MaterialInstance = font_material, .ModelMatrix = transformA });
            // g_Renderer->AddRenderable({ .EntityId = 2, .GeometryId = geometry->InternalID, .MaterialInstance = font_material, .ModelMatrix = transformB });
            g_Renderer->AddRenderable(background);
            g_Renderer->AddRenderable({
                .ModelMatrix = transformB,
                .MaterialInstance = font_material,
                .GeometryId = active_points_array->geometry->InternalID,
                .EntityId = 420,
            });

            g_Renderer->AddRenderable(play_button_text);
            g_Renderer->AddRenderable(settings_button_text);

            // g_Renderer->AddRenderable(font_bitmap);
            // g_Renderer->Draw(bg_material->Shader, &global_data, geometry->InternalID);
            g_Renderer->Draw(&global_data);

            // Draw ImGui
            app_state.imgui_renderer.BeginFrame();
            ImGui::ShowDemoWindow(&imgui_demo_window);
            DrawGameUI(game_state);

            ImGui::Begin("TextRenderDebug");
            // ImGui::DragFloat("Wave amplitude", &wave_amplitude);
            // ImGui::DragFloat("Wave speed", &wave_speed);
            ImGui::DragFloat("Animation Speed", &animation_speed);

            static Vec2f scale = { 1.0f, 1.0f };
            static Vec2f position = { -270.0f, -270.0f };
            if (ImGui::DragFloat2("Position", position._data))
            {
                transformB = ScaleMatrix(Vec3f{ scale.x, scale.y, 1.f }) * TranslationMatrix(Vec3f{ position.x, position.y, 0.f });
            }

            if (ImGui::DragFloat2("Scale", scale._data))
            {
                transformB = ScaleMatrix(Vec3f{ scale.x, scale.y, 1.f }) * TranslationMatrix(Vec3f{ position.x, position.y, 0.f });
            }

            if (ImGui::Button("Switch Path"))
            {
                if (active_points_array == &points1_array)
                {
                    active_points_array = &points2_array;
                }
                else
                {
                    active_points_array = &points1_array;
                }

                for (u32 i = 0; i < KRAFT_C_ARRAY_SIZE(character_states); i++)
                {
                    // character_states[i].current_point_index = i;
                    character_states[i].progress = 0.1f;
                }
            }

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
