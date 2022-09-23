#pragma once

#include "core/kraft_core.h"
#include "platform/windows/kraft_win32_window.h"

namespace kraft
{

struct Window;

struct ApplicationConfig
{
    size_t      WindowWidth;
    size_t      WindowHeight;
    const char* ApplicationName;
    const char* WindowTitle;
};

struct ApplicationState
{
    kraft::Window Window;
};

struct Application
{
    ApplicationConfig Config;
    ApplicationState State;

    bool Running = false;
    bool Suspended = false;

    // These methods must be implemented by the application itself
    bool (*Init)();
    void (*Update)(float64 deltaTime);
    void (*Render)(float64 deltaTime);
    void (*OnResize)(size_t width, size_t height);
    void (*OnBackground)();
    void (*OnForeground)();
    bool (*Shutdown)();

    bool Create();
    bool Run();
    void Destroy();
};

}