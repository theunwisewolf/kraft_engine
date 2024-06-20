#include "kraft_application.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_input.h"
#include "core/kraft_time.h"
#include "core/kraft_string.h"
#include "containers/array.h"
#include "platform/kraft_window.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_filesystem.h"

#if defined(KRAFT_GUI_APP)
#include "systems/kraft_texture_system.h"
#include "systems/kraft_material_system.h"
#include "systems/kraft_geometry_system.h"
#include "systems/kraft_shader_system.h"
#include "renderer/kraft_renderer_frontend.h"
#endif

namespace kraft
{

Application* Application::I = nullptr;

bool Application::WindowResizeListener(EventType type, void* sender, void* listener, EventData data)
{
    uint32 width = data.UInt32Value[0];
    uint32 height = data.UInt32Value[1];

    Application* application = (Application*)listener;
    application->Config.WindowWidth = width;
    application->Config.WindowHeight = height;

    if (width == 0 && height == 0)
    {
        KINFO("[Application::WindowResizeListener]: Application suspended");
        application->Suspended = true;   
    }
    else
    {
        if (application->Suspended)
        {
            KINFO("[Application::WindowResizeListener]: Application resumed");
            application->Suspended = false;
        }

        application->OnResize(width, height);

#if defined(KRAFT_GUI_APP)
        application->State.Renderer.OnResize(width, height);
#endif
    }

    return false;
}

bool Application::Create(int argc, char *argv[])
{
    CommandLineArgs = {};
    CommandLineArgs.Count = argc;

    CommandLineArgs.Arguments = Array<String>(argc);
    for (int i = 0; i < argc; i++)
    {
        CommandLineArgs.Arguments[i] = argv[i];
    }

    BasePath = filesystem::Dirname(CommandLineArgs.Arguments[0]);
    BasePath = filesystem::CleanPath(BasePath);

    I = this;
    Platform::Init(&this->Config);
    EventSystem::Init();
    InputSystem::Init();

#if defined(KRAFT_GUI_APP)
    if (!State.Renderer.Init(&this->Config))
    {
        KERROR("[Application::Create]: Failed to initalize renderer!");
        return false;
    }
    
    TextureSystem::Init(256);
    MaterialSystem::Init(MaterialSystemConfig{256});
    GeometrySystem::Init(GeometrySystemConfig{256});
    ShaderSystem::Init(256);
#endif

    if (!this->Init())
    {
        return false;
    }

    this->Running = true;
    this->Suspended = false;

#if defined(KRAFT_GUI_APP)
    this->OnResize(this->Config.WindowWidth, this->Config.WindowHeight);

    EventSystem::Listen(EventType::EVENT_TYPE_WINDOW_RESIZE, this, Application::WindowResizeListener);
#endif
    return true;
}

bool Application::Run()
{
    kraft::Time::Start();
    State.LastTime = kraft::Time::ElapsedTime;
    float64 targetFrameRate = 1 / 144.f;
    uint64 frames = 0;
    float64 timeSinceLastSecond = 0.f;

    kraft::PrintDebugMemoryInfo();
    char windowTitleBuffer[1024];

#if defined(KRAFT_CONSOLE_APP)
    if (this->Update)
    {
        while (this->Running)
        {
            kraft::Time::Update();
            float64 currentTime = kraft::Time::ElapsedTime;
            float64 deltaTime = currentTime - State.LastTime;
            kraft::Time::DeltaTime = deltaTime;
            timeSinceLastSecond += deltaTime;
            
            // TODO
            // this->Running = ??
            this->Update(deltaTime);
            
            if (timeSinceLastSecond >= 1.f)
            {
                timeSinceLastSecond = 0.f;
                frames = 0;
            }

            if (deltaTime < targetFrameRate)
            {
                Platform::SleepMilliseconds((targetFrameRate - deltaTime) * 1000.f);
            }

            State.LastTime = currentTime;
        }
    }
#else
    while (this->Running)
    {
        kraft::Time::Update();
        float64 currentTime = kraft::Time::ElapsedTime;
        float64 deltaTime = currentTime - State.LastTime;
        kraft::Time::DeltaTime = deltaTime;
        timeSinceLastSecond += deltaTime;

        this->Running = Platform::PollEvents();

        // If the app is not suspended, update & render
        if (!this->Suspended)
        {
            renderer::RenderPacket Packet;
            Packet.DeltaTime = deltaTime;

            this->Update(deltaTime);
            this->Render(deltaTime, Packet);

            State.Renderer.DrawFrame(&Packet);

            InputSystem::Update(deltaTime);
        }

        if (timeSinceLastSecond >= 1.f)
        {
            timeSinceLastSecond = 0.f;

            StringFormat(windowTitleBuffer, sizeof(windowTitleBuffer), "%s (%d fps | %f ms frametime)", Config.WindowTitle, frames, deltaTime);
            Platform::GetWindow().SetWindowTitle(windowTitleBuffer);

            frames = 0;
        }

        if (deltaTime < targetFrameRate)
        {
            // KINFO("Before Sleep - %f ms | Sleep time = %f", kraft::Platform::GetAbsoluteTime(), (targetFrameRate - deltaTime) * 1000.f);
            Platform::SleepMilliseconds((targetFrameRate - deltaTime) * 1000.f);
            // KINFO("After Sleep - %f ms", kraft::Platform::GetAbsoluteTime());
        }

        State.LastTime = currentTime;
        frames++;
    }
#endif

    this->Destroy();
    return true;
}

void Application::Destroy()
{
    KINFO("[Application::Destroy]: Shutting down...");

    InputSystem::Shutdown();
    EventSystem::Shutdown();
    Platform::Shutdown();

#if defined(KRAFT_GUI_APP)
    MaterialSystem::Shutdown();
    TextureSystem::Shutdown();
    ShaderSystem::Shutdown();
    GeometrySystem::Shutdown();
    State.Renderer.Shutdown();
#endif

    this->Shutdown();
    Time::Stop();

    KSUCCESS("[Application::Destroy]: Application shutdown successfully!");
}

}