#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "math/kraft_math.h"

#define KRAFT_TEXTURE_NAME_MAX_LENGTH 512
#define KRAFT_MATERIAL_NAME_MAX_LENGTH 512

namespace kraft
{

typedef uint32 ResourceID;

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
    ResourceID  ID;
    char        Name[KRAFT_MATERIAL_NAME_MAX_LENGTH];
    TextureMap  DiffuseMap;
    Vec4f       DiffuseColor;
};

struct Geometry
{
    ResourceID  ID;

    // For backend mapping
    ResourceID  InternalID;

    String      Name;
    Material*   Material;
};

struct RenderResource
{
    String Name;

    // Backend-specific layout
    void* VulkanDescriptorSetLayout;
    void* VulkanDescriptorSet;
};

struct CommandBuffer
{
    void *Handle;
};

struct RenderPipeline
{
    // Backend-specific pipeline
    void*            Pipeline;
    void*            PipelineLayout;
    RenderResource   Resources;

    void Bind(CommandBuffer CommandBuffer);
    void Destroy();
};

}