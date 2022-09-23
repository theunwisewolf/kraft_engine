#pragma once

#include "core/kraft_log.h"
#include "core/kraft_application.h"

extern bool CreateApplication(kraft::Application* app);

int main()
{
    kraft::Application app;
    if (!CreateApplication(&app))
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

    if (!app.Create())
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