#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "math/kraft_math.h"
#include "containers/kraft_array.h"

namespace kraft
{

struct ApplicationConfig;
struct Texture;
struct Material;
struct Geometry;
struct Shader;
struct ShaderUniform;

namespace renderer
{

struct ShaderEffect;

struct GlobalUniformData
{
    union
    {
        struct
        {
            Mat4f Projection;
            Mat4f View;
        };

        char _[256];
    };

    GlobalUniformData() {}
};

struct GeometryRenderData
{
    Mat4f       ModelMatrix;
    Geometry*   Geometry;
};

struct RenderPacket
{
    float64 DeltaTime;
    Array<Material*>          MaterialInstances;
    Array<GeometryRenderData> Geometries;
};

enum RendererBackendType
{
    RENDERER_BACKEND_TYPE_NONE,
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,

    RENDERER_BACKEND_TYPE_NUM_COUNT
};

struct RenderResource
{
    String Name;

    // Backend-specific layout
    // VulkanShaderResource for Vulkan
};

struct RendererBackend
{
    bool (*Init)(ApplicationConfig* config);
    bool (*Shutdown)();
    bool (*BeginFrame)(float64 deltaTime);
    bool (*EndFrame)(float64 deltaTime);
    void (*OnResize)(int width, int height);

    // Texture
    void (*CreateTexture)(uint8* data, Texture* out);
    void (*DestroyTexture)(Texture* texture);

    // Shader
    void (*UseShader)(const Shader* Shader);
    void (*SetUniform)(Shader* Shader, const ShaderUniform& Uniform, void* Value, bool Invalidate);
    void (*ApplyGlobalShaderProperties)(Shader* Shader);
    void (*ApplyInstanceShaderProperties)(Shader* Shader);
    void (*CreateRenderPipeline)(Shader* Shader, int PassIndex);
    void (*DestroyRenderPipeline)(Shader* Shader);
    void (*CreateMaterial)(Material *material);
    void (*DestroyMaterial)(Material *material);

    // Geometry
    void (*DrawGeometryData)(GeometryRenderData Data);
    bool (*CreateGeometry)(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize);
    void (*DestroyGeometry)(Geometry* Geometry);
};

struct RendererImguiBackend
{
    bool (*Init)();
    bool (*Destroy)();
    bool (*BeginFrame)(float64 deltaTime);
    bool (*EndFrame)(float64 deltaTime);
    void* (*AddTexture)(Texture* Texture);
};

struct Vertex3D
{
    Vec3f Position;
    Vec2f UV;
};

namespace ShaderDataType
{
    enum Enum
    {
        Float, Float2, Float3, Float4, Mat4, Byte, Byte4N, UByte, UByte4N, Short2, Short2N, Short4, Short4N, UInt, UInt2, UInt4, Count 
    };

    static const char* Strings[] = 
    {
        "Float", "Float2", "Float3", "Float4", "Mat4", "Byte", "Byte4N", "UByte", "UByte4N", "Short2", "Short2N", "Short4", "Short4N", "UInt", "UInt2", "UInt4", "Count"
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }

    static uint64 SizeOf(Enum Value)
    {
        switch (Value)
        {
            case Float:  return 1 * sizeof(float32);
            case Float2: return 2 * sizeof(float32);
            case Float3: return 3 * sizeof(float32);
            case Float4: return 4 * sizeof(float32);
            case Mat4:   return 16 * sizeof(float32);
            case Byte:   return 1 * sizeof(byte);
            case Byte4N: return 4 * sizeof(byte);
            case UByte:  return 4 * sizeof(byte);
            case UByte4N:return 4 * sizeof(byte);
            case Short2: return 2 * sizeof(int16);
            case Short2N:return 2 * sizeof(int16);
            case Short4: return 4 * sizeof(int16);
            case Short4N:return 4 * sizeof(int16);
            case UInt:   return 1 * sizeof(uint32);
            case UInt2:  return 2 * sizeof(uint32);
            case UInt4:  return 4 * sizeof(uint32);
            case Count:  return 0;
        }

        return 0;
    }
}

namespace VertexInputRate
{
    enum Enum
    {
        PerVertex, PerInstance, Count
    };

    static const char* Strings[] = 
    {
        "PerVertex", "PerInstance", "Count"
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
}

namespace ResourceType
{
    enum Enum
    {
        Sampler, UniformBuffer, ConstantBuffer, Count
    };

    static const char* Strings[] =
    {
        "Sampler", "UniformBuffer", "ConstantBuffer", "Count"
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
}

namespace PolygonMode
{
    enum Enum
    {
        Fill, Line, Point, Count
    };

    static const char* Strings[] =
    {
        "Fill", "Line", "Point"
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
}

namespace CullMode
{
    enum Enum
    {
        None, Front, Back, FrontAndBack, Count
    };

    static const char* Strings[] =
    {
        "None", "Front", "Back", "FrontAndBack", "Count",
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
}

namespace CompareOp
{
    enum Enum
    {
        Never, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always, Count,
    };

    static const char* Strings[] =
    {
        "Never", "Less", "Equal", "LessOrEqual", "Greater", "NotEqual", "GreaterOrEqual", "Always", "Count",
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
}

namespace BlendFactor
{
    enum Enum
    {
        Zero, One, SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
        SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha, Count
    };

    static const char* Strings[] =
    {
        "Zero", "One", "SrcColor", "OneMinusSrcColor", "DstColor", "OneMinusDstColor",
        "SrcAlpha", "OneMinusSrcAlpha", "DstAlpha", "OneMinusDstAlpha", "Count",
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
}

namespace BlendOp
{
    enum Enum
    {
        Add, Subtract, ReverseSubtract, Min, Max, Count
    };

    static const char* Strings[] =
    {
        "Add", "Subtract", "ReverseSubtract", "Min", "Max", "Count"
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
}

struct BlendState
{
    BlendFactor::Enum SrcColorBlendFactor;
    BlendFactor::Enum DstColorBlendFactor;
    BlendOp::Enum     ColorBlendOperation;

    BlendFactor::Enum SrcAlphaBlendFactor;
    BlendFactor::Enum DstAlphaBlendFactor;
    BlendOp::Enum     AlphaBlendOperation;
};

namespace ShaderStage
{
    enum Enum
    {
        Vertex, Geometry, Fragment, Compute, Count
    };

    static const char* Strings[] = 
    {
        "Vertex", "Geometry", "Fragment", "Compute", "Count"
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
};

namespace ShaderUniformScope
{
    enum Enum
    {
        Global, Instance, Local, Count
    };

    static const char* Strings[] =
    {
        "Global", "Instance", "Local", "Count"  
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
    }
};

}

}