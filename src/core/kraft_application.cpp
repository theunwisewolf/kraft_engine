#include "kraft_application.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_input.h"
#include "core/kraft_time.h"
#include "platform/kraft_platform.h"
#include "renderer/kraft_renderer_frontend.h"

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
        KINFO("Application suspended");
        application->Suspended = true;   
    }
    else
    {
        if (application->Suspended)
        {
            KINFO("Application resumed");
            application->Suspended = false;
        }

        application->OnResize(width, height);
        application->State.Renderer.OnResize(width, height);
    }

    return false;
}

bool Application::Create()
{
    I = this;
    Platform::Init(&this->Config);
    EventSystem::Init();
    InputSystem::Init();

    RendererFrontend::I = &State.Renderer;
    if (!State.Renderer.Init(&this->Config))
    {
        KERROR("[Application::Create]: Failed to initalize renderer!");
        return false;
    }

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

    kraft::PrintDebugMemoryInfo();

    while (this->Running)
    {
        kraft::Time::Update();
        float64 currentTime = kraft::Time::ElapsedTime;
        float64 deltaTime = currentTime - State.LastTime;

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

        // if (deltaTime < targetFrameRate)
        // {
            // KINFO("Before Sleep - %f ms | Sleep time = %f", kraft::Platform::GetAbsoluteTime(), (targetFrameRate - deltaTime) * 1000.f);
            // Platform::SleepMilliseconds((targetFrameRate - deltaTime) * 1000.f);
            // KINFO("After Sleep - %f ms", kraft::Platform::GetAbsoluteTime());
        // }

        State.LastTime = currentTime;
    }

    this->Destroy();
    return true;
}

void Application::Destroy()
{
    KINFO("Shutting down...");

    this->Shutdown();
    Time::Stop();
    Platform::Shutdown();
    State.Renderer.Shutdown();

    KSUCCESS("Application shutdown successfully!");
}

}