#pragma once

#include "core/kraft_core.h"
#include "math/kraft_math.h"
#include "resources/kraft_resource_types.h"

namespace kraft
{

struct ApplicationConfig;

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
    void (*CreateMaterial)(Material *material);
    void (*DestroyMaterial)(Material *material);
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
};

struct Vertex3D
{
    Vec3f Position;
    Vec2f UV;
};

namespace renderer
{

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

enum CullMode
{
    CULL_MODE_NONE,
    CULL_MODE_FRONT,
    CULL_MODE_BACK,
    CULL_MODE_FRONT_AND_BACK,

    CULL_MODE_COUNT
};

enum ZTestOp
{
    ZTEST_OP_NEVER = 0,
    ZTEST_OP_LESS = 1,
    ZTEST_OP_EQUAL = 2,
    ZTEST_OP_LESS_OR_EQUAL = 3,
    ZTEST_OP_GREATER = 4,
    ZTEST_OP_NOT_EQUAL = 5,
    ZTEST_OP_GREATER_OR_EQUAL = 6,
    ZTEST_OP_ALWAYS = 7,

    ZTEST_OP_COUNT,
};

enum BlendFactor
{
    BLEND_FACTOR_ZERO = 0,
    BLEND_FACTOR_ONE = 1,
    BLEND_FACTOR_SRC_COLOR = 2,
    BLEND_FACTOR_ONE_MINUS_SRC_COLOR = 3,
    BLEND_FACTOR_DST_COLOR = 4,
    BLEND_FACTOR_ONE_MINUS_DST_COLOR = 5,
    BLEND_FACTOR_SRC_ALPHA = 6,
    BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 7,
    BLEND_FACTOR_DST_ALPHA = 8,
    BLEND_FACTOR_ONE_MINUS_DST_ALPHA = 9,

    BLEND_FACTOR_COUNT
};

enum BlendOp
{
    BLEND_OP_ADD = 0,
    BLEND_OP_SUBTRACT = 1,
    BLEND_OP_REVERSE_SUBTRACT = 2,
    BLEND_OP_MIN = 3,
    BLEND_OP_MAX = 4,

    BLEND_OP_COUNT
};

struct BlendState
{
    BlendFactor SrcColorBlendFactor;
    BlendFactor DstColorBlendFactor;
    BlendOp     ColorBlendOperation;

    BlendFactor SrcAlphaBlendFactor;
    BlendFactor DstAlphaBlendFactor;
    BlendOp     AlphaBlendOperation;
};

namespace ShaderStage
{
    enum Enum
    {
        SHADER_STAGE_VERTEX,
        SHADER_STAGE_GEOMETRY,
        SHADER_STAGE_FRAGMENT,
        SHADER_STAGE_COMPUTE,

        SHADER_STAGE_COUNT
    };

    static const char* Strings[] = 
    {
        "Vertex", "Geometry", "Fragment", "Compute", "Count"
    };

    static const char* String(Enum Value)
    {
        return (Value < Enum::SHADER_STAGE_COUNT ? Strings[(int)Value] : "Unsupported");
    }
};



}

}