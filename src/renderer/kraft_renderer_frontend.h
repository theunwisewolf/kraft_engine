#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft
{

struct RendererFrontend
{
    static RendererFrontend* I;

    RendererBackend* Backend = nullptr;
    Block BackendMemory;
    RendererBackendType Type;

    bool Init();
    bool Shutdown();
    void OnResize(int width, int height);
    bool DrawFrame(RenderPacket* packet);
};

}