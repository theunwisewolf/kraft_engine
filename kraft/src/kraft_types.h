#pragma once

#include <core/kraft_string.h>

namespace kraft {

struct EngineConfig
{
    int         Argc = 0;
    char**      Argv = 0;
    const char* ApplicationName = "";
    bool        ConsoleApp = false;
};

struct CreateWindowOptions
{
    String Title = "";
    uint32 Width = 0;
    uint32 Height = 0;
    int    RenderererHint = 1;
    bool   Primary = true;
};

#if defined(KRAFT_GUI_APP)

enum RendererBackendType : int
{
    RENDERER_BACKEND_TYPE_NONE,
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,

    RENDERER_BACKEND_TYPE_NUM_COUNT
};

struct CreateRendererOptions
{
    RendererBackendType Backend = RENDERER_BACKEND_TYPE_VULKAN;
    bool                VSync = false;
    uint16              GlobalUBOSizeInBytes = 128;
    uint16              TextureCacheSize = 256;
};

#endif

} // namespace kraft