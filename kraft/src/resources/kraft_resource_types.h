#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"

#define KRAFT_TEXTURE_NAME_MAX_LENGTH 512
#define KRAFT_MATERIAL_NAME_MAX_LENGTH 512

namespace kraft
{

struct Texture
{
    uint32 ID;
    uint32 Width;
    uint32 Height;
    uint8  Channels;
    uint32 Generation;
    char  Name[KRAFT_TEXTURE_NAME_MAX_LENGTH];

    void *RendererData; // Renderer specific data
};

enum TextureUse
{
    TEXTURE_USE_UNKNOWN = 0x00,
    TEXTURE_USE_MAP_DIFFUSE = 0x01,
};

struct TextureMap
{
    Texture*    Texture;
    TextureUse  Usage;
};

struct Material
{
    uint32      ID;
    char       Name[KRAFT_MATERIAL_NAME_MAX_LENGTH];
    TextureMap  DiffuseMap;
    Vec4f       DiffuseColor;
};

}