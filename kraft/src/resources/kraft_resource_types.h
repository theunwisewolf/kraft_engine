#pragma once

#include "renderer/kraft_renderer_types.h"
#include "shaderfx/kraft_shaderfx_types.h"

#define KRAFT_MATERIAL_NAME_MAX_LENGTH 256
#define KRAFT_GEOMETRY_NAME_MAX_LENGTH 256
#define KRAFT_MATERIAL_MAX_INSTANCES   128

namespace kraft {

typedef u32 ResourceID;

struct Material;

namespace r {
struct RenderPass;
}

struct ShaderUniform
{
    u32                         Location;
    u32                         Offset;
    u32                         Stride;
    r::ShaderDataType           DataType;
    r::ShaderUniformScope::Enum Scope;
};

struct Shader
{
    ResourceID             ID;
    String8                Path;
    shaderfx::ShaderEffect ShaderEffect;
    Array<ShaderUniform>   UniformCache;
    HashMap<u64, u32>      UniformCacheMapping;

    r::Handle<r::RenderPass> RenderPassHandle = r::Handle<r::RenderPass>::Invalid(); // The renderpass of the shader's pipeline
    void*                    RendererData;
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
    u32                        ID;
    f32                        Width;
    f32                        Height;
    u8                         Channels;
    r::Format::Enum            TextureFormat;
    r::TextureSampleCountFlags SampleCount;
    TextureMapType             Type;

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
    KRAFT_INLINE MaterialProperty(u32 Index, Type Val)                                                                                                                                                 \
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
        Mat4f              Mat4fValue;
        Vec4f              Vec4fValue;
        Vec3f              Vec3fValue;
        Vec2f              Vec2fValue;
        f32                f32Value;
        f64                Float64Value;
        uint8              UInt8Value;
        u16                UInt16Value;
        u32                u32Value;
        u64                UInt64Value;
        r::Handle<Texture> TextureValue;

        char Memory[128];
    };

    // Index of the uniform in the shader uniform cache
    u16 UniformIndex;
    u16 Size = 0;

    MaterialProperty()
    {
        MemSet(Memory, 0, sizeof(Memory));
    }

    MATERIAL_PROPERTY_SETTERS(Mat4f);
    MATERIAL_PROPERTY_SETTERS(Vec4f);
    MATERIAL_PROPERTY_SETTERS(Vec3f);
    MATERIAL_PROPERTY_SETTERS(Vec2f);
    MATERIAL_PROPERTY_SETTERS(f32);
    MATERIAL_PROPERTY_SETTERS(f64);
    MATERIAL_PROPERTY_SETTERS(u8);
    MATERIAL_PROPERTY_SETTERS(u16);
    MATERIAL_PROPERTY_SETTERS(u32);
    MATERIAL_PROPERTY_SETTERS(u64);
    MATERIAL_PROPERTY_SETTERS(r::Handle<Texture>);

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
    ResourceID                         ID;
    String8                            Name;
    String8                            AssetPath;
    FlatHashMap<u64, MaterialProperty> Properties;

    // Reference to the underlying shader
    Shader* Shader;
};

struct Geometry
{
    ResourceID ID;

    // For backend mapping
    ResourceID InternalID;
};

} // namespace kraft