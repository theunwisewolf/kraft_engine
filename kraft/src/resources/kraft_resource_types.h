#pragma once

#include "renderer/kraft_renderer_types.h"
#include "shaderfx/kraft_shaderfx_types.h"

#define KRAFT_MATERIAL_NAME_MAX_LENGTH 256
#define KRAFT_GEOMETRY_NAME_MAX_LENGTH 256
#define KRAFT_MATERIAL_MAX_INSTANCES   128

namespace kraft {

typedef uint32 ResourceID;

using namespace renderer;

struct Material;

namespace renderer {
struct RenderPass;
}

struct ShaderUniform
{
    uint32                   Location;
    uint32                   Offset;
    uint32                   Stride;
    ShaderDataType           DataType;
    ShaderUniformScope::Enum Scope;
};

struct Shader
{
    ResourceID              ID;
    String                  Path;
    shaderfx::ShaderEffect  ShaderEffect;
    Array<ShaderUniform>    UniformCache;
    HashMap<String, uint32> UniformCacheMapping;

    Material*          ActiveMaterial;                                   // The shader will read instance properties from this material when bound
    Handle<RenderPass> RenderPassHandle = Handle<RenderPass>::Invalid(); // The renderpass of the shader's pipeline
    void*              RendererData;
};

enum TextureMapType : uint8
{
    TEXTURE_MAP_TYPE_DIFFUSE,
    TEXTURE_MAP_TYPE_SPECULAR,
    TEXTURE_MAP_TYPE_NORMAL,
    TEXTURE_MAP_TYPE_HEIGHT,
};

struct Texture
{
    uint32                  ID;
    float32                 Width;
    float32                 Height;
    uint8                   Channels;
    Format::Enum            TextureFormat;
    TextureSampleCountFlags SampleCount;
    TextureMapType          Type;

    char DebugName[255];
};

struct TextureSampler
{};

#define MATERIAL_PROPERTY_SET(Type)                                                                                                                                                                    \
    KRAFT_INLINE void Set(Type Val)                                                                                                                                                                    \
    {                                                                                                                                                                                                  \
        MemCpy(this->Memory, (void*)&Val, sizeof(Type));                                                                                                                                               \
    }
#define MATERIAL_PROPERTY_CONSTRUCTOR(Type)                                                                                                                                                            \
    KRAFT_INLINE MaterialProperty(uint32 Index, Type Val)                                                                                                                                              \
    {                                                                                                                                                                                                  \
        this->UniformIndex = Index;                                                                                                                                                                    \
        MemCpy(this->Memory, (void*)&Val, sizeof(Type));                                                                                                                                               \
    }
#define MATERIAL_PROPERTY_OPERATOR(Type)                                                                                                                                                               \
    KRAFT_INLINE MaterialProperty* operator=(Type Val)                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        MemCpy(this->Memory, (void*)&Val, sizeof(Type));                                                                                                                                               \
        return this;                                                                                                                                                                                   \
    }
#define MATERIAL_PROPERTY_SETTERS(Type)                                                                                                                                                                \
    MATERIAL_PROPERTY_CONSTRUCTOR(Type)                                                                                                                                                                \
    MATERIAL_PROPERTY_OPERATOR(Type)                                                                                                                                                                   \
    MATERIAL_PROPERTY_SET(Type)

struct MaterialProperty
{
    union
    {
        Mat4f           Mat4fValue;
        Vec4f           Vec4fValue;
        Vec3f           Vec3fValue;
        Vec2f           Vec2fValue;
        float32         Float32Value;
        float64         Float64Value;
        uint8           UInt8Value;
        uint16          UInt16Value;
        uint32          UInt32Value;
        uint64          UInt64Value;
        Handle<Texture> TextureValue;

        char Memory[128];
    };

    // Index of the uniform in the shader uniform cache
    uint16 UniformIndex;
    uint16 Size = 0;

    MaterialProperty()
    {
        MemSet(Memory, 0, sizeof(Memory));
    }

    MATERIAL_PROPERTY_SETTERS(Mat4f);
    MATERIAL_PROPERTY_SETTERS(Vec4f);
    MATERIAL_PROPERTY_SETTERS(Vec3f);
    MATERIAL_PROPERTY_SETTERS(Vec2f);
    MATERIAL_PROPERTY_SETTERS(float32);
    MATERIAL_PROPERTY_SETTERS(float64);
    MATERIAL_PROPERTY_SETTERS(uint8);
    MATERIAL_PROPERTY_SETTERS(uint16);
    MATERIAL_PROPERTY_SETTERS(uint32);
    MATERIAL_PROPERTY_SETTERS(uint64);
    MATERIAL_PROPERTY_SETTERS(Handle<Texture>);

    template<typename T>
    T Get() const
    {
        return *((T*)(&Memory[0]));
    }
};

#undef MATERIAL_PROPERTY_SET
#undef MATERIAL_PROPERTY_CONSTRUCTOR
#undef MATERIAL_PROPERTY_OPERATOR
#undef MATERIAL_PROPERTY_SETTERS

struct Material
{
    ResourceID                            ID;
    String                                Name;
    String                                AssetPath;
    FlatHashMap<String, MaterialProperty> Properties;

    // Reference to the underlying shader
    Shader* Shader;

    // Holds the backend renderer data such as descriptor sets
    ResourceID RendererDataIdx;
};

struct Geometry
{
    ResourceID ID;

    // For backend mapping
    ResourceID InternalID;
};

} // namespace kraft