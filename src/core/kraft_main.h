#pragma once

#include "core/kraft_log.h"
#include "core/kraft_application.h"

extern bool CreateApplication(kraft::Application* app, int argc, char *argv[]);

#if defined(KRAFT_PLATFORM_WINDOWS) && !defined(KRAFT_DEBUG)
#include "Windows.h"
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
#if defined(KRAFT_PLATFORM_WINDOWS) && !defined(KRAFT_DEBUG)
    int argc = __argc;
    char **argv = __argv;
#endif

    kraft::Application app;
    if (!CreateApplication(&app, argc, argv))
    {
        KFATAL("Failed to create application!");
        return -1;
    }

    if (!app.Init ||
        !app.Update ||
        !app.Render ||
        !app.OnResize ||
        !app.OnBackground ||
        !app.OnForeground ||
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