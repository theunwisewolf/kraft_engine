#include "kraft_application.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_time.h"
#include "platform/kraft_platform.h"
#include "renderer/kraft_renderer_frontend.h"

namespace kraft
{

bool Application::Create()
{
    Platform::Init(&this->Config);
    EventSystem::Init();
    InputSystem::Init();

    State.Renderer.Type = this->Config.RendererBackend;
    RendererFrontend::I = &State.Renderer;

    if (!State.Renderer.Init())
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
            packet.deltaTime = deltaTime;
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