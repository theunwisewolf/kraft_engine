#pragma once

#include <core/kraft_string.h>
#include <platform/kraft_window_types.h>

namespace kraft {

//
// --- GUI Only Code ---
//

#if defined(KRAFT_GUI_APP)

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
    u16                 GlobalUBOSizeInBytes = 128;
    u16                 TextureCacheSize = 512;
    u16                 MaxMaterials = 1024;
    u16                 MaterialBufferSize = 64; // Maximum size of a single material in bytes
    u8                  MSAASamples = 1;         // 1 = no MSAA, 2/4/8 = MSAA sample count
};

#endif

} // namespace kraft