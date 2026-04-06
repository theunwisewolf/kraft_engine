#include "kraft_engine.h"

#include "kraft_includes.h"

namespace kraft {

struct EngineState {
    ArenaAllocator* arena;
    String8Array cli_args;
}* internal_state;

static EngineStats s_EngineStats = {};

struct EngineConfig Engine::config = {};
bool Engine::running = false;
bool Engine::suspended = false;
String8 Engine::base_path = {};

static EngineTickStats& CurrentTickStats() {
    return s_EngineStats.current_tick;
}

static void BeginTickStatsFrame() {
    if (s_EngineStats.current_tick.frame_id != 0) {
        s_EngineStats.last_completed_tick = s_EngineStats.current_tick;
    }

    s_EngineStats.current_tick = {};
    s_EngineStats.current_tick.frame_id = ++s_EngineStats.tick_counter;
}

#if defined(KRAFT_GUI_APP)
static bool WindowResizeListener(EventType Type, void* Sender, void* Listener, EventData event_data) {
    EventDataResize data = *(EventDataResize*)(&event_data);
    u32 Width = data.width;
    u32 Height = data.height;

    if (Width == 0 && Height == 0) {
        KINFO("[Engine]: Application suspended");
        Engine::suspended = true;
    } else {
        if (Engine::suspended) {
            KINFO("[Engine]: Application resumed");
            Engine::suspended = false;
        }

        g_Renderer->OnResize(Width, Height);
    }

    return false;
}
#endif

bool Engine::Init(const EngineConfig* config) {
    ArenaAllocator* arena = ArenaAlloc();
    MemCpy(&Engine::config, config, sizeof(EngineConfig));

    internal_state = ArenaPush(arena, EngineState);
    internal_state->arena = arena;
    internal_state->cli_args.ptr = ArenaPushArray(arena, String8, config->argc);
    internal_state->cli_args.count = config->argc;

    for (int i = 0; i < config->argc; i++) {
        internal_state->cli_args.ptr[i] = ArenaPushString8Copy(arena, String8FromCString(config->argv[i]));
    }

    base_path = fs::Dirname(arena, internal_state->cli_args.ptr[0]);
    base_path = fs::CleanPath(arena, base_path);

    Platform::Init(&Engine::config);
    EventSystem::Init(arena);
    InputSystem::Init();

#if defined(KRAFT_GUI_APP)
    EventSystem::Listen(EventType::EVENT_TYPE_WINDOW_RESIZE, nullptr, WindowResizeListener);
#endif

    Engine::running = true;
    Engine::suspended = false;

    Time::Start();

    return true;
}

bool Engine::Tick() {
    BeginTickStatsFrame();
    EngineTickStats& tick_stats = CurrentTickStats();
    KRAFT_TIMING_STATS_BEGIN(engine_tick_total_timer);

    KRAFT_TIMING_STATS_BEGIN(time_update_timer);
    Time::Update();
    KRAFT_TIMING_STATS_ACCUM(time_update_timer, tick_stats.time_update_ms);

#if defined(KRAFT_GUI_APP)
    KRAFT_TIMING_STATS_BEGIN(input_update_timer);
    InputSystem::Update();
    KRAFT_TIMING_STATS_ACCUM(input_update_timer, tick_stats.input_update_ms);

    KRAFT_TIMING_STATS_BEGIN(poll_events_timer);
    Engine::running = Platform::PollEvents();
    KRAFT_TIMING_STATS_ACCUM(poll_events_timer, tick_stats.poll_events_ms);
    tick_stats.poll_events_result = Engine::running;
#endif

    KRAFT_TIMING_STATS_SET(engine_tick_total_timer, tick_stats.total_ms);

    return true;
}

bool Engine::Present() {
#if defined(KRAFT_GUI_APP)
    // TODO (amn): This should happen at the end of the frame
    // Clear out the resources
    // r::ResourceManager->EndFrame(0);

    // return Engine::Renderer.DrawFrame();
#endif

    return false;
}

void Engine::Destroy() {
    KINFO("[Engine]: Shutting down...");

    InputSystem::Shutdown();
    EventSystem::Shutdown();
    Platform::Shutdown();

    Time::Stop();

    KSUCCESS("[Engine]: Engine shutdown complete");
}

const String8Array Engine::GetCommandLineArgs() {
    return internal_state->cli_args;
}

const EngineStats& Engine::GetStats() {
    return s_EngineStats;
}

} // namespace kraft
