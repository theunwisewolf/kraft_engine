#pragma once

#include "core/kraft_core.h"

namespace kraft::renderer {
struct RendererFrontend;

enum RendererBackendType : int;
}

namespace kraft {

struct Window;

struct RendererSettings
{
    bool   VSync = false;
	uint16 GlobalUBOSizeInBytes = 128;
};

struct EngineConfigT
{
    uint32                        WindowWidth;
    uint32                        WindowHeight;
    const char*                   WindowTitle;
    const char*                   ApplicationName;
    renderer::RendererBackendType RendererBackend;
    bool                          ConsoleApp;
    bool                          StartMaximized;
    RendererSettings              RendererSettings = {};
};

struct KRAFT_API Engine
{
    static EngineConfigT Config;

#if defined(KRAFT_GUI_APP)
    static renderer::RendererFrontend Renderer;
#endif

    // The location of the executable without a trailing slash
    static String BasePath;

    // "true" if the application is running and no exit event was recorded
    static bool Running;
    // "true" if the application is the background/minimized
    static bool Suspended;

    // Initializes the engine
    static bool Init(int argc, char* argv[], EngineConfigT Config);

    // Ticks one frame
    static bool Tick();

    // Draws the frame
    static bool Present();

    // Shutdown
    static void Destroy();

    static const Array<String>& GetCommandLineArgs();
};

}