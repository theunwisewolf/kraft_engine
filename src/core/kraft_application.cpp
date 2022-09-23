#include "kraft_application.h"

#include "core/kraft_log.h"
#include "core/kraft_memory.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "platform/kraft_platform.h"
#include "platform/windows/kraft_win32_window.h"

namespace kraft
{

bool Application::Create()
{
    Platform::Init();
    EventSystem::Init();
    InputSystem::Init();

    State.Window = kraft::Window();
    State.Window.Init(this->Config.WindowTitle, this->Config.WindowWidth, this->Config.WindowHeight);

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
    kraft::PrintDebugMemoryInfo();

    while (this->Running)
    {
        this->Running = State.Window.PollEvents();

        // If the app is not suspended, update & render
        if (!this->Suspended)
        {
            this->Update(0);
            this->Render(0);

            InputSystem::Update(0);
        }
    }

    this->Destroy();
    return true;
}

void Application::Destroy()
{
    KINFO("Shutting down...");

    this->Shutdown();
    kraft::Platform::Shutdown();

    KSUCCESS("Application shutdown successfully!");
}

}