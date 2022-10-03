#pragma once

#include "core/kraft_core.h"

namespace kraft
{

enum RendererBackendType
{
    RENDERER_BACKEND_TYPE_NONE,
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,

    RENDERER_BACKEND_TYPE_NUM_COUNT
};

struct RendererBackend
{
    bool (*Init)();
    bool (*Shutdown)();
    bool (*BeginFrame)(float64 deltaTime);
    bool (*EndFrame)(float64 deltaTime);
    void (*OnResize)(int width, int height);
};

struct RenderPacket
{
    float64 deltaTime;
};

}