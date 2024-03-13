#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"
#include "containers/kraft_array.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

struct ApplicationConfig;

namespace renderer
{

struct ShaderEffect;

struct GeometryRenderData
{
    Mat4f       ModelMatrix;
    Geometry*   Geometry;
};

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

    // Texture
    void (*CreateTexture)(uint8* data, Texture* out);
    void (*DestroyTexture)(Texture* texture);

    // Shader
    RenderPipeline (*CreateRenderPipeline)(const ShaderEffect& Effect, int PassIndex);
    void (*CreateMaterial)(Material *material);
    void (*DestroyMaterial)(Material *material);

    // Geometry
    void (*DrawGeometryData)(GeometryRenderData Data);
    bool (*CreateGeometry)(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 IndexCount, const void* Indices);
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

struct RenderPacket
{
    float64 DeltaTime;
    Array<GeometryRenderData> Geometries;
};

struct Vertex3D
{
    Vec3f Position;
    Vec2f UV;
};

namespace VertexAttributeFormat
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
        Sampler, UniformBuffer, Count
    };

    static const char* Strings[] =
    {
        "Sampler", "UniformBuffer", "Count"
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

}

}