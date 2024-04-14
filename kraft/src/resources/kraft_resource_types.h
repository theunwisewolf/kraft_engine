#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "containers/kraft_buffer.h"
#include "containers/kraft_hashmap.h"
#include "math/kraft_math.h"
#include "renderer/shaderfx/kraft_shaderfx_types.h"
#include "renderer/kraft_renderer_types.h"

#define KRAFT_TEXTURE_NAME_MAX_LENGTH 256
#define KRAFT_MATERIAL_NAME_MAX_LENGTH 256
#define KRAFT_GEOMETRY_NAME_MAX_LENGTH 256

namespace kraft
{

typedef uint32 ResourceID;

using namespace renderer;

struct ShaderUniform
{
    uint32                   Location;
    uint32                   Offset;
    uint32                   Stride;
    ResourceType::Enum       Type;
    ShaderUniformScope::Enum Scope;
};

struct Shader
{
    ResourceID                      ID;
    String                          Path;
    renderer::ShaderEffect          ShaderEffect;
    Array<ShaderUniform>            UniformCache;
    Array<Texture*>                 Textures;
    HashMap<String, uint32>         UniformCacheMapping;
    uint32                          InstanceUniformsOffset;
    uint32                          InstanceUniformsCount;
    uint32                          InstanceUBOOffset;
    uint32                          GlobalUBOOffset;
    uint64                          InstanceUBOStride;
    uint64                          GlobalUBOStride;

    void*                           RendererData;
};

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

struct MaterialInternalData
{
    HashMap<String, uint32> ResourceMapping;
};

struct Material
{
    ResourceID      ID;
    char            Name[KRAFT_MATERIAL_NAME_MAX_LENGTH];
    TextureMap      DiffuseMap;
    Vec4f           DiffuseColor;
    bool            Dirty;

    // Reference to the underlying shader
    Shader*         Shader;
};

struct Geometry
{
    ResourceID  ID;

    // For backend mapping
    ResourceID  InternalID;

    char        Name[KRAFT_GEOMETRY_NAME_MAX_LENGTH];
};

}