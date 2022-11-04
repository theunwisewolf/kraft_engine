#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"

namespace kraft
{

struct ApplicationConfig;

enum RendererBackendType
{
    RENDERER_BACKEND_TYPE_NONE,
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,

    RENDERER_BACKEND_TYPE_NUM_COUNT
};

struct RendererBackend
{
    bool (*Init)(ApplicationConfig* config);
    bool (*Shutdown)();
    bool (*BeginFrame)(float64 deltaTime);
    bool (*EndFrame)(float64 deltaTime);
    void (*OnResize)(int width, int height);
};

struct RenderPacket
{
    float64 DeltaTime;
};

struct Vertex3D
{
    Vec3f Position;
    Vec4f Color;
};

}