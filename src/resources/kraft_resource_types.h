#pragma once

#include "core/kraft_core.h"

namespace kraft
{

struct Texture
{
    uint32 ID;
    uint32 Width;
    uint32 Height;
    uint8  Channels;
    uint32 Generation;

    void *RendererData; // Renderer specific data
};

}