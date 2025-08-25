#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct EngineConfig
{
    int     argc = 0;
    char**  argv = 0;
    String8 application_name = {};
    bool    console_app = false;
};

struct KRAFT_API Engine
{
    static EngineConfig config;

    // The location of the executable without a trailing slash
    static String8 base_path;

    // "true" if the application is running and no exit event was recorded
    static bool running;

    // "true" if the application is the background/minimized
    static bool suspended;

    // Initializes the engine
    static bool Init(const EngineConfig* config);

    // Ticks one frame
    static bool Tick();

    // Draws the frame
    static bool Present();

    // Shutdown
    static void Destroy();

    static const String8Array GetCommandLineArgs();
};

} // namespace kraft