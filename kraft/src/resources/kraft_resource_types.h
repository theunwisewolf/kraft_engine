#pragma once

#include "renderer/shaderfx/kraft_shaderfx_types.h"
#include "renderer/kraft_renderer_types.h"

#define KRAFT_TEXTURE_NAME_MAX_LENGTH  256
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
    ResourceType::Enum       Type;
    ShaderDataType::Enum     DataType;
    ShaderUniformScope::Enum Scope;
};

struct Shader
{
    ResourceID              ID;
    String                  Path;
    renderer::ShaderEffect  ShaderEffect;
    Array<ShaderUniform>    UniformCache;
    HashMap<String, uint32> UniformCacheMapping;
    uint32                  TextureCount;
    uint32                  GlobalUBOOffset;
    uint32                  GlobalUBOStride;
    uint32                  InstanceUniformsCount;
    uint32                  InstanceUBOStride; // The stride will be same for all material instances

    Material*          ActiveMaterial; // The shader will read instance properties from this material when bound
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

    uint32 Generation;
    char   Name[KRAFT_TEXTURE_NAME_MAX_LENGTH];

    void* RendererData; // Renderer specific data
};

#define MATERIAL_PROPERTY_SET(Type)                                                                                                        \
    KRAFT_INLINE void Set(Type Val)                                                                                                        \
    {                                                                                                                                      \
        MemCpy(this->Memory, (void*)&Val, sizeof(Type));                                                                                   \
    }
#define MATERIAL_PROPERTY_CONSTRUCTOR(Type)                                                                                                \
    KRAFT_INLINE MaterialProperty(uint32 Index, Type Val)                                                                                  \
    {                                                                                                                                      \
        this->UniformIndex = Index;                                                                                                        \
        MemCpy(this->Memory, (void*)&Val, sizeof(Type));                                                                                   \
    }
#define MATERIAL_PROPERTY_OPERATOR(Type)                                                                                                   \
    KRAFT_INLINE MaterialProperty* operator=(Type Val)                                                                                     \
    {                                                                                                                                      \
        MemCpy(this->Memory, (void*)&Val, sizeof(Type));                                                                                   \
        return this;                                                                                                                       \
    }
#define MATERIAL_PROPERTY_SETTERS(Type)                                                                                                    \
    MATERIAL_PROPERTY_CONSTRUCTOR(Type)                                                                                                    \
    MATERIAL_PROPERTY_OPERATOR(Type)                                                                                                       \
    MATERIAL_PROPERTY_SET(Type)

struct MaterialProperty
{
    union
    {
        Mat4f           Mat4fValue;
        Vec4f           Vec4fValue;
        Vec3f           Vec3fValue;
        Vec2f           Vec2fValue;
        Float32         Float32Value;
        Float64         Float64Value;
        UInt8           UInt8Value;
        UInt16          UInt16Value;
        UInt32          UInt32Value;
        UInt64          UInt64Value;
        Handle<Texture> TextureValue;

        char Memory[128];
    };

    // Index of the uniform in the shader uniform cache
    uint32 UniformIndex;

    MaterialProperty()
    {
        MemSet(Memory, 0, sizeof(Memory));
    }

    MATERIAL_PROPERTY_SETTERS(Mat4f);
    MATERIAL_PROPERTY_SETTERS(Vec4f);
    MATERIAL_PROPERTY_SETTERS(Vec3f);
    MATERIAL_PROPERTY_SETTERS(Vec2f);
    MATERIAL_PROPERTY_SETTERS(Float32);
    MATERIAL_PROPERTY_SETTERS(Float64);
    MATERIAL_PROPERTY_SETTERS(UInt8);
    MATERIAL_PROPERTY_SETTERS(UInt16);
    MATERIAL_PROPERTY_SETTERS(UInt32);
    MATERIAL_PROPERTY_SETTERS(UInt64);
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
    ResourceID ID;
    String     Name;
    String     AssetPath;
    bool       Dirty;

    // Reference to the underlying shader
    Shader* Shader;

    // List of textures of this material instance
    Array<Handle<Texture>> Textures;

    // All the material properties
    // The indices match to the material's properties hashmap
    HashMap<String, MaterialProperty> Properties;

    // Holds the backend renderer data such as descriptor sets
    void* RendererData;

    template<typename T>
    T GetUniform(const String& Name) const
    {
        return Properties.find(Name).value().Get<T>();
    }
};

struct Geometry
{
    ResourceID ID;

    // For backend mapping
    ResourceID InternalID;

    char Name[KRAFT_GEOMETRY_NAME_MAX_LENGTH];
};

}