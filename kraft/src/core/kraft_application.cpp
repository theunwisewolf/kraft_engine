#include "kraft_application.h"

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
#include "systems/kraft_geometry_system.h"
#include "systems/kraft_material_system.h"
#include "systems/kraft_shader_system.h"
#include "systems/kraft_texture_system.h"
#endif

namespace kraft {

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

bool Application::Create(int argc, char* argv[])
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
    MaterialSystem::Init(MaterialSystemConfig{ 256 });
    GeometrySystem::Init(GeometrySystemConfig{ 256 });
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
    float64 TargetFrameTime = 1 / 60.f;
    uint64  FrameCount = 0;
    float64 timeSinceLastFrame = 0.f;

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
        float64 currentTime = kraft::Platform::GetAbsoluteTime();
        float64 deltaTime = currentTime - State.LastTime;
        timeSinceLastFrame += deltaTime;
        kraft::Time::DeltaTime = deltaTime;
        State.LastTime = currentTime;

        this->Running = Platform::PollEvents();

        float64 FrameStartTime = kraft::Platform::GetAbsoluteTime();
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
        float64 FrameEndTime = kraft::Platform::GetAbsoluteTime();
        float64 FrameTime = FrameEndTime - FrameStartTime;

        if (timeSinceLastFrame >= 1.f)
        {
            StringFormat(
                windowTitleBuffer,
                sizeof(windowTitleBuffer),
                "%s (%d fps | %f ms deltaTime | %f ms targetFrameTime)",
                Config.WindowTitle,
                FrameCount,
                deltaTime * 1000.f,
                TargetFrameTime * 1000.0f
            );
            Platform::GetWindow().SetWindowTitle(windowTitleBuffer);

            timeSinceLastFrame = 0.f;
            FrameCount = 0;
        }

        if (FrameTime < TargetFrameTime)
        {
            float64 CurrentAbsoluteTime = kraft::Platform::GetAbsoluteTime();
            // KINFO("Before Sleep - %f s | Sleep time = %f ms", kraft::Platform::GetAbsoluteTime(), (targetFrameRate - deltaTime) * 1000.f);
            Platform::SleepMilliseconds((uint64)((TargetFrameTime - FrameTime) * 1000.f));
            // KINFO("After Sleep - %f s", kraft::Platform::GetAbsoluteTime());
            // KINFO(
            //     "Slept for - %f ms | Asked for - %f ms",
            //     (kraft::Platform::GetAbsoluteTime() - CurrentAbsoluteTime) * 1000.f,
            //     (TargetFrameTime - FrameTime) * 1000.f
            // );
        }

        FrameCount++;
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