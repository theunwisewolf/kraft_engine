#pragma once

#include "core/kraft_core.h"
#include "core/kraft_events.h"
#include "core/kraft_string.h"
#include "containers/kraft_array.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/kraft_renderer_frontend.h"

namespace kraft
{

struct Window;

struct ApplicationConfig
{
    uint32                          WindowWidth;
    uint32                          WindowHeight;
    const char*                     ApplicationName;
    const char*                     WindowTitle;
    renderer::RendererBackendType   RendererBackend = renderer::RendererBackendType::RENDERER_BACKEND_TYPE_NONE;
    bool                            ConsoleApp;
};

struct ApplicationState
{
    renderer::RendererFrontend Renderer;
    float64          LastTime;
};

struct ApplicationCommandLineArgs
{
    int             Count;
    Array<String>   Arguments;
};

struct KRAFT_API Application
{
    static Application* I;
    inline static Application* Get() { return I; };

    ApplicationConfig Config = {};
    ApplicationState  State = {};
    ApplicationCommandLineArgs CommandLineArgs = {};

    // The location of the executable without a trailing slash
    String BasePath;
    bool Running = false;
    bool Suspended = false;

    // These methods must be implemented by the application itself
    bool (*Init)();
    void (*Update)(float64 DeltaTime);
    void (*Render)(float64 DeltaTime, renderer::RenderPacket& RenderPacket);
    void (*OnResize)(size_t Width, size_t Height);
    void (*OnBackground)();
    void (*OnForeground)();
    bool (*Shutdown)();

    bool Create(int argc, char *argv[]);
    bool Run();
    void Destroy();
    static bool WindowResizeListener(EventType type, void* sender, void* listener, EventData data);
};

}