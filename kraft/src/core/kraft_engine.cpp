#include "kraft_engine.h"

#include "containers/array.h"
#include "core/kraft_input.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_string.h"
#include "core/kraft_time.h"
#include "platform/kraft_filesystem.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_window.h"

#if defined(KRAFT_GUI_APP)
#include "renderer/kraft_renderer_frontend.h"
#include "renderer/kraft_resource_manager.h"
#include "systems/kraft_geometry_system.h"
#include "systems/kraft_material_system.h"
#include "systems/kraft_shader_system.h"
#include "systems/kraft_texture_system.h"
#endif

namespace kraft {

EngineConfig               Engine::Config = {};
CommandLineArgs            Engine::CommandLineArgs = {};
renderer::RendererFrontend Engine::Renderer = {};
bool                       Engine::Running = false;
bool                       Engine::Suspended = false;
String                     Engine::BasePath = {};

static bool WindowResizeListener(EventType Type, void* Sender, void* Listener, EventData Data)
{
    uint32 Width = Data.UInt32Value[0];
    uint32 Height = Data.UInt32Value[1];

    Engine::Config.WindowWidth = Width;
    Engine::Config.WindowHeight = Height;

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

#if defined(KRAFT_GUI_APP)
        Engine::Renderer.OnResize(Width, Height);
#endif
    }

    return false;
}

bool Engine::Init(int argc, char* argv[], EngineConfig Config)
{
    Engine::Config = Config;
    CommandLineArgs = {};
    CommandLineArgs.Count = argc;

    CommandLineArgs.Arguments = Array<String>(argc);
    for (int i = 0; i < argc; i++)
    {
        CommandLineArgs.Arguments[i] = argv[i];
    }

    BasePath = filesystem::Dirname(CommandLineArgs.Arguments[0]);
    BasePath = filesystem::CleanPath(BasePath);

    Platform::Init(&Engine::Config);
    EventSystem::Init();
    InputSystem::Init();

#if defined(KRAFT_GUI_APP)
    if (!Engine::Renderer.Init(&Engine::Config))
    {
        KERROR("[Application::Create]: Failed to initalize renderer!");
        return false;
    }

    TextureSystem::Init(256);
    MaterialSystem::Init({ .MaxMaterialsCount = 256 });
    GeometrySystem::Init({ .MaxGeometriesCount = 256 });
    ShaderSystem::Init(256);

    EventSystem::Listen(EventType::EVENT_TYPE_WINDOW_RESIZE, nullptr, WindowResizeListener);
    if (Engine::Config.StartMaximized)
    {
        Platform::GetWindow().Maximize();
    }
#endif

    Engine::Running = true;
    Engine::Suspended = false;

    Time::Start();

    return true;
}

bool Engine::Tick()
{
    Time::Update();
#if defined(KRAFT_CONSOLE_APP)

#else
    InputSystem::Update();
    Engine::Running = Platform::PollEvents();
#endif

    return true;
}

bool Engine::Present()
{
    // TODO (amn): This should happen at the end of the frame
    // Clear out the resources
    renderer::ResourceManager::Ptr->EndFrame(0);

#if defined(KRAFT_GUI_APP)
    return Engine::Renderer.DrawFrame();
#endif

    return false;
}

void Engine::Destroy()
{
    KINFO("[Engine]: Shutting down...");

    InputSystem::Shutdown();
    EventSystem::Shutdown();
    Platform::Shutdown();

#if defined(KRAFT_GUI_APP)
    MaterialSystem::Shutdown();
    TextureSystem::Shutdown();
    ShaderSystem::Shutdown();
    GeometrySystem::Shutdown();
    Engine::Renderer.Shutdown();
#endif

    Time::Stop();

    KSUCCESS("[Engine]: Engine shutdown complete");
}

}