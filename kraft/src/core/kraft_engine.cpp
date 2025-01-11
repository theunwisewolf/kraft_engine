#include "kraft_engine.h"

#include <containers/kraft_array.h>
#include <core/kraft_events.h>
#include <core/kraft_input.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>
#include <core/kraft_time.h>
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
    Array<String> CliArgs;
} InternalState;

struct EngineConfig Engine::Config = {};
bool                Engine::Running = false;
bool                Engine::Suspended = false;
String              Engine::BasePath = {};

#if defined(KRAFT_GUI_APP)
static bool WindowResizeListener(EventType Type, void* Sender, void* Listener, EventData Data)
{
    uint32 Width = Data.UInt32Value[0];
    uint32 Height = Data.UInt32Value[1];

    if (Width == 0 && Height == 0)
    {
        KINFO("[Engine]: Application suspended");
        Engine::Suspended = true;
    }
    else
    {
        if (Engine::Suspended)
        {
            KINFO("[Engine]: Application resumed");
            Engine::Suspended = false;
        }

        g_Renderer->OnResize(Width, Height);
    }

    return false;
}
#endif

bool Engine::Init(const EngineConfig& Config)
{
    MemCpy(&Engine::Config, &Config, sizeof(EngineConfig));

    InternalState.CliArgs = Array<String>(Config.Argc);
    for (int i = 0; i < Config.Argc; i++)
    {
        InternalState.CliArgs[i] = Config.Argv[i];
    }

    BasePath = filesystem::Dirname(InternalState.CliArgs[0]);
    BasePath = filesystem::CleanPath(BasePath);

    Platform::Init(&Engine::Config);
    EventSystem::Init();
    InputSystem::Init();

#if defined(KRAFT_GUI_APP)
    EventSystem::Listen(EventType::EVENT_TYPE_WINDOW_RESIZE, nullptr, WindowResizeListener);
#endif

    Engine::Running = true;
    Engine::Suspended = false;

    Time::Start();

    return true;
}

bool Engine::Tick()
{
    Time::Update();

#if defined(KRAFT_GUI_APP)
    InputSystem::Update();
    Engine::Running = Platform::PollEvents();
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

const Array<String>& Engine::GetCommandLineArgs()
{
    return InternalState.CliArgs;
}

} // namespace kraft