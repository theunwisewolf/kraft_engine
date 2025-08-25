#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct EngineConfig;

struct KRAFT_API Engine
{
    static EngineConfig Config;

    // The location of the executable without a trailing slash
    static String BasePath;

    // "true" if the application is running and no exit event was recorded
    static bool Running;

    // "true" if the application is the background/minimized
    static bool Suspended;

    // Initializes the engine
    static bool Init(const EngineConfig& Config);

    // Ticks one frame
    static bool Tick();

    // Draws the frame
    static bool Present();

    // Shutdown
    static void Destroy();

    static const Array<String>& GetCommandLineArgs();
};

} // namespace kraft