#include "kraft_engine.h"

#include <containers/kraft_array.h>
#include <platform/kraft_filesystem.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <systems/kraft_asset_database.h>

#if defined(KRAFT_GUI_APP)
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/kraft_resource_manager.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>
#endif

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
    uint32          Width = data.width;
    uint32          Height = data.height;

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

    for (int i = 0; i < config->argc; i++)
    {
        internal_state->cli_args.ptr[i] = ArenaPushString8Copy(arena, String8FromCString(config->argv[i]));
    }

    base_path = filesystem::Dirname(arena, internal_state->cli_args.ptr[0]);
    base_path = filesystem::CleanPath(arena, base_path);

    Platform::Init(&Engine::config);
    EventSystem::Init();
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
    // renderer::ResourceManager->EndFrame(0);

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