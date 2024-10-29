#pragma once

#include "containers/kraft_array.h"
#include "core/kraft_core.h"
#include "core/kraft_events.h"
#include "core/kraft_string.h"
#include "renderer/kraft_renderer_frontend.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft {

struct Window;

struct EngineConfig
{
    uint32                        WindowWidth;
    uint32                        WindowHeight;
    const char*                   WindowTitle;
    const char*                   ApplicationName;
    renderer::RendererBackendType RendererBackend = renderer::RendererBackendType::RENDERER_BACKEND_TYPE_NONE;
    bool                          ConsoleApp;
    bool                          StartMaximized;
};

struct CommandLineArgs
{
    int           Count;
    Array<String> Arguments;
};

struct KRAFT_API Engine
{
    static EngineConfig               Config;
    static CommandLineArgs            CommandLineArgs;
    static renderer::RendererFrontend Renderer;

    // The location of the executable without a trailing slash
    static String BasePath;

    // "true" if the application is running and no exit event was recorded
    static bool Running;
    // "true" if the application is the background/minimized
    static bool Suspended;

    // Initializes the engine
    static bool Init(int argc, char* argv[], EngineConfig Config);

    // Ticks one frame
    static bool Tick();

    // Draws the frame
    static bool Present();

    // Shutdown
    static void Destroy();
};

}