#pragma once

#include "core/kraft_log.h"
#include "core/kraft_application.h"

extern bool CreateApplication(kraft::Application* app, int argc, char *argv[]);

#if defined(KRAFT_PLATFORM_WINDOWS) && !defined(KRAFT_DEBUG) && defined(KRAFT_GUI_APP)
#define KRAFT_USE_WINMAIN 1
#else 
#define KRAFT_USE_WINMAIN 0
#endif

#if KRAFT_USE_WINMAIN
#include "Windows.h"
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
#if KRAFT_USE_WINMAIN
    int argc = __argc;
    char **argv = __argv;
#endif

    kraft::Application app = {0};
    if (!CreateApplication(&app, argc, argv))
    {
        KFATAL("Failed to create application!");
        return -1;
    }

    if (!app.Init ||
#if defined(KRAFT_GUI_APP)
        !app.Update ||
        !app.Render ||
        !app.OnResize ||
        !app.OnBackground ||
        !app.OnForeground ||
#endif
        !app.Shutdown)
    {
        KFATAL("The application does not define all of the required methods!");
        return -2;
    }

    if (!app.Create(argc, argv))
    {
        KFATAL("Application creation failed!");
        return -3;
    }

    if (!app.Run())
    {
        KFATAL("Application did not shutdown gracefully!");
        return -4;
    }

    return 0;
}