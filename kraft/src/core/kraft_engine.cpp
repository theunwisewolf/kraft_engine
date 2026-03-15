#include "kraft_engine.h"

#include <kraft_types.h>

namespace kraft {

struct EngineState
{
    ArenaAllocator* arena;
    String8Array    cli_args;
}* internal_state;

struct EngineConfig Engine::config = {};
bool                Engine::running = false;
bool                Engine::suspended = false;
String8             Engine::base_path = {};

#if defined(KRAFT_GUI_APP)
static bool WindowResizeListener(EventType Type, void* Sender, void* Listener, EventData event_data)
{
    EventDataResize data = *(EventDataResize*)(&event_data);
    u32             Width = data.width;
    u32             Height = data.height;

    if (Width == 0 && Height == 0)
    {
        KINFO("[Engine]: Application suspended");
        Engine::suspended = true;
    }
    else
    {
        if (Engine::suspended)
        {
            KINFO("[Engine]: Application resumed");
            Engine::suspended = false;
        }

        g_Renderer->OnResize(Width, Height);
    }

    return false;
}
#endif

bool Engine::Init(const EngineConfig* config)
{
    ArenaAllocator* arena = ArenaAlloc();
    MemCpy(&Engine::config, config, sizeof(EngineConfig));

    internal_state = ArenaPush(arena, EngineState);
    internal_state->arena = arena;
    internal_state->cli_args.ptr = ArenaPushArray(arena, String8, config->argc);
    internal_state->cli_args.count = config->argc;

    for (int i = 0; i < config->argc; i++)
    {
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

bool Engine::Tick()
{
    Time::Update();

#if defined(KRAFT_GUI_APP)
    InputSystem::Update();
    Engine::running = Platform::PollEvents();
#endif

    return true;
}

bool Engine::Present()
{
#if defined(KRAFT_GUI_APP)
    // TODO (amn): This should happen at the end of the frame
    // Clear out the resources
    // r::ResourceManager->EndFrame(0);

    // return Engine::Renderer.DrawFrame();
#endif

    return false;
}

void Engine::Destroy()
{
    KINFO("[Engine]: Shutting down...");

    InputSystem::Shutdown();
    EventSystem::Shutdown();
    Platform::Shutdown();

    Time::Stop();

    KSUCCESS("[Engine]: Engine shutdown complete");
}

const String8Array Engine::GetCommandLineArgs()
{
    return internal_state->cli_args;
}

} // namespace kraft