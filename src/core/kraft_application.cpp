#include "kraft_application.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_input.h"
#include "core/kraft_time.h"
#include "core/kraft_string.h"
#include "containers/array.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_filesystem.h"
#include "renderer/kraft_renderer_frontend.h"
#include "systems/kraft_texture_system.h"

namespace kraft
{

Application* Application::I = nullptr;

bool Application::WindowResizeListener(EventType type, void* sender, void* listener, EventData data)
{
    uint32 width = data.UInt32[0];
    uint32 height = data.UInt32[1];

    Application* application = (Application*)listener;
    application->Config.WindowWidth = width;
    application->Config.WindowHeight = height;

    if (width == 0 && height == 0)
    {
        KINFO(TEXT("[Application::WindowResizeListener]: Application suspended"));
        application->Suspended = true;   
    }
    else
    {
        if (application->Suspended)
        {
            KINFO(TEXT("[Application::WindowResizeListener]: Application resumed"));
            application->Suspended = false;
        }

        application->OnResize(width, height);
        application->State.Renderer.OnResize(width, height);
    }

    return false;
}

bool Application::Create(int argc, char *argv[])
{
    CommandLineArgs = {};
    CommandLineArgs.Count = argc;

    CommandLineArgs.Arguments = Array<TString>(argc);
    for (int i = 0; i < argc; i++)
    {
        CommandLineArgs.Arguments[i] = ANSI_TO_TCHAR(argv[i]);
    }

    BasePath = filesystem::Basename(CommandLineArgs.Arguments[0]);
    BasePath = filesystem::CleanPath(BasePath);

    I = this;
    Platform::Init(&this->Config);
    EventSystem::Init();
    InputSystem::Init();

    if (!State.Renderer.Init(&this->Config))
    {
        KERROR(TEXT("[Application::Create]: Failed to initalize renderer!"));
        return false;
    }

    TextureSystem::Init(256);

    if (!this->Init())
    {
        return false;
    }

    this->Running = true;
    this->Suspended = false;
    this->OnResize(this->Config.WindowWidth, this->Config.WindowHeight);

    EventSystem::Listen(EventType::EVENT_TYPE_WINDOW_RESIZE, this, Application::WindowResizeListener);
    return true;
}

bool Application::Run()
{
    kraft::Time::Start();
    State.LastTime = kraft::Time::ElapsedTime;
    float64 targetFrameRate = 1 / 60.f;
    uint64 frames = 0;
    float64 timeSinceLastSecond = 0.f;

    kraft::PrintDebugMemoryInfo();
    TCHAR windowTitleBuffer[1024];
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
            this->Update(deltaTime);
            this->Render(deltaTime);

            RenderPacket packet;
            packet.DeltaTime = deltaTime;
            State.Renderer.DrawFrame(&packet);

            InputSystem::Update(deltaTime);
        }

        if (timeSinceLastSecond >= 1.f)
        {
            timeSinceLastSecond = 0.f;

            StringFormat(windowTitleBuffer, sizeof(windowTitleBuffer), TEXT("%s (%d fps | %f ms frametime)"), Config.WindowTitle, frames, deltaTime);
            Platform::GetWindow().SetWindowTitle(windowTitleBuffer);
            frames = 0;
        }

        if (deltaTime < targetFrameRate)
        {
            // KINFO(TEXT("Before Sleep - %f ms | Sleep time = %f"), kraft::Platform::GetAbsoluteTime(), (targetFrameRate - deltaTime) * 1000.f);
            Platform::SleepMilliseconds((targetFrameRate - deltaTime) * 1000.f);
            // KINFO(TEXT("After Sleep - %f ms"), kraft::Platform::GetAbsoluteTime());
        }

        State.LastTime = currentTime;
        frames++;
    }

    this->Destroy();
    return true;
}

void Application::Destroy()
{
    KINFO(TEXT("[Application::Destroy]: Shutting down..."));

    InputSystem::Shutdown();
    EventSystem::Shutdown();
    Platform::Shutdown();
    TextureSystem::Shutdown();
    State.Renderer.Shutdown();

    this->Shutdown();
    Time::Stop();

    KSUCCESS(TEXT("[Application::Destroy]: Application shutdown successfully!"));
}

}