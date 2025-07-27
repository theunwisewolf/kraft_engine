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

//
// --- GUI Only Code ---
//

#if defined(KRAFT_GUI_APP)

struct WindowOptions
{
    String Title = "";
    uint32 Width = 0;
    uint32 Height = 0;
    int    RenderererHint = 1;
    bool   Primary = true;
    bool   StartMaximized = false;
};

enum RendererBackendType : int
{
    RENDERER_BACKEND_TYPE_NONE,
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,

    RENDERER_BACKEND_TYPE_NUM_COUNT
};

struct RendererOptions
{
    RendererBackendType Backend = RENDERER_BACKEND_TYPE_VULKAN;
    bool                VSync = false;
    uint16              GlobalUBOSizeInBytes = 128;
    uint16              TextureCacheSize = 512;
    uint16              MaxMaterials = 1024;
    uint16              MaterialBufferSize = 64; // Maximum size of a single material in bytes
};

#endif

} // namespace kraft