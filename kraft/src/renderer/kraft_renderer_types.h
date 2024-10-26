#pragma once

#include "containers/kraft_array.h"
#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "imgui/imgui.h"
#include "math/kraft_math.h"

struct ImDrawData;

namespace kraft {

struct ApplicationConfig;
struct Texture;
struct Material;
struct Geometry;
struct Shader;
struct ShaderUniform;

namespace renderer {

// Templated only for type-safety
template<typename T> struct Handle
{
private:
    uint16 Index;
    uint16 Generation;

    template<typename U, typename V> friend struct Pool;
    Handle(uint16 Index, uint16 Generation) : Index(Index), Generation(Generation) {}

public:
    Handle() : Index(0), Generation(0) {}
    static Handle Invalid() { return Handle(0, 0xffff); }
    bool          IsInvalid() { return *this == Handle::Invalid(); }

    bool operator==(const Handle<T> Other) { return Other.Generation == Generation && Other.Index == Index; }
    bool operator!=(const Handle<T> Other) { return Other.Generation != Generation || Other.Index != Index; }
};

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

struct Renderable
{
    kraft::Mat4f ModelMatrix;
    Material*    MaterialInstance;
    uint32       GeometryID;
};

struct RenderPacket
{
    float64 DeltaTime;
    Mat4f   ProjectionMatrix;
    Mat4f   ViewMatrix;
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
    bool (*PrepareFrame)();
    bool (*BeginFrame)(float64 deltaTime);
    bool (*EndFrame)(float64 deltaTime);
    void (*OnResize)(int width, int height);

    void (*BeginSceneView)();
    void (*EndSceneView)();
    void (*OnSceneViewViewportResize)(uint32 width, uint32 height);
    bool (*SetSceneViewViewportSize)(uint32 Width, uint32 Height);
    Handle<Texture> (*GetSceneViewTexture)();

    // Shader
    void (*UseShader)(const Shader* Shader);
    void (*SetUniform)(Shader* Shader, const ShaderUniform& Uniform, void* Value, bool Invalidate);
    void (*ApplyGlobalShaderProperties)(Shader* ActiveShader);
    void (*ApplyInstanceShaderProperties)(Shader* ActiveShader);
    void (*CreateRenderPipeline)(Shader* Shader, int PassIndex);
    void (*DestroyRenderPipeline)(Shader* Shader);
    void (*CreateMaterial)(Material* Material);
    void (*DestroyMaterial)(Material* Material);

    // Geometry
    void (*DrawGeometryData)(uint32 GeometryID);
    bool (*CreateGeometry)(
        Geometry*    Geometry,
        uint32       VertexCount,
        const void*  Vertices,
        uint32       VertexSize,
        uint32       IndexCount,
        const void*  Indices,
        const uint32 IndexSize
    );
    void (*DestroyGeometry)(Geometry* Geometry);
};

struct RendererImguiBackend
{
    bool (*Init)();
    bool (*Destroy)();
    bool (*BeginFrame)(float64 deltaTime);
    bool (*EndFrame)(ImDrawData* DrawData);
    ImTextureID (*AddTexture)(Handle<Texture> Resource);
    void (*RemoveTexture)(ImTextureID TextureID);
    void (*PostFrameCleanup)();
};

struct Vertex3D
{
    Vec3f Position;
    Vec2f UV;
};

namespace ShaderDataType {
enum Enum
{
    Float,
    Float2,
    Float3,
    Float4,
    Mat4,
    Byte,
    Byte4N,
    UByte,
    UByte4N,
    Short2,
    Short2N,
    Short4,
    Short4N,
    UInt,
    UInt2,
    UInt4,
    Count
};

static const char* Strings[] = { "Float",  "Float2",  "Float3", "Float4",  "Mat4", "Byte",  "Byte4N", "UByte", "UByte4N",
                                 "Short2", "Short2N", "Short4", "Short4N", "UInt", "UInt2", "UInt4",  "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}

static uint64 SizeOf(Enum Value)
{
    switch (Value)
    {
        case Float:   return 1 * sizeof(float32);
        case Float2:  return 2 * sizeof(float32);
        case Float3:  return 3 * sizeof(float32);
        case Float4:  return 4 * sizeof(float32);
        case Mat4:    return 16 * sizeof(float32);
        case Byte:    return 1 * sizeof(byte);
        case Byte4N:  return 4 * sizeof(byte);
        case UByte:   return 4 * sizeof(byte);
        case UByte4N: return 4 * sizeof(byte);
        case Short2:  return 2 * sizeof(int16);
        case Short2N: return 2 * sizeof(int16);
        case Short4:  return 4 * sizeof(int16);
        case Short4N: return 4 * sizeof(int16);
        case UInt:    return 1 * sizeof(uint32);
        case UInt2:   return 2 * sizeof(uint32);
        case UInt4:   return 4 * sizeof(uint32);
        case Count:   return 0;
    }

    return 0;
}
}

namespace VertexInputRate {
enum Enum
{
    PerVertex,
    PerInstance,
    Count
};

static const char* Strings[] = { "PerVertex", "PerInstance", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

namespace ResourceType {
enum Enum
{
    Sampler,
    UniformBuffer,
    ConstantBuffer,
    Count
};

static const char* Strings[] = { "Sampler", "UniformBuffer", "ConstantBuffer", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

namespace PolygonMode {
enum Enum
{
    Fill,
    Line,
    Point,
    Count
};

static const char* Strings[] = { "Fill", "Line", "Point" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

namespace CompareOp {
enum Enum
{
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
    Count,
};

static const char* Strings[] = {
    "Never", "Less", "Equal", "LessOrEqual", "Greater", "NotEqual", "GreaterOrEqual", "Always", "Count",
};

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

namespace BlendFactor {
enum Enum
{
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    Count
};

static const char* Strings[] = {
    "Zero",     "One",
    "SrcColor", "OneMinusSrcColor",
    "DstColor", "OneMinusDstColor",
    "SrcAlpha", "OneMinusSrcAlpha",
    "DstAlpha", "OneMinusDstAlpha",
    "Count",
};

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

namespace BlendOp {
enum Enum
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
    Count
};

static const char* Strings[] = { "Add", "Subtract", "ReverseSubtract", "Min", "Max", "Count" };

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

namespace ShaderUniformScope {
enum Enum
{
    Global,
    Instance,
    Local,
    Count
};

static const char* Strings[] = { "Global", "Instance", "Local", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace Format {
enum Enum
{
    // Color Formats
    RED,
    RGBA8_UNORM,
    RGB8_UNORM,
    BGRA8_UNORM,
    BGR8_UNORM,
    // Depth-Stencil Formats
    D16_UNORM,
    D32_SFLOAT,
    D16_UNORM_S8_UINT,
    D24_UNORM_S8_UINT,
    D32_SFLOAT_S8_UINT,
    Count
};

static const char* Strings[] = { "RED",        "RGBA8_UNORM",       "RGB8_UNORM",        "BGRA8_UNORM",        "BGR8_UNORM", "D16_UNORM",
                                 "D32_SFLOAT", "D16_UNORM_S8_UINT", "D24_UNORM_S8_UINT", "D32_SFLOAT_S8_UINT", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace TextureTiling {
enum Enum
{
    Optimal,
    Linear,
    Count
};

static const char* Strings[] = { "Optimal", "Linear", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace TextureType {
enum Enum
{
    Type1D,
    Type2D,
    Type3D,
    Count
};

static const char* Strings[] = { "Type1D", "Type2D", "Type3D", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace TextureFilter {
enum Enum
{
    Nearest,
    Linear,
    Count
};

static const char* Strings[] = { "Nearest", "Linear", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace TextureWrapMode {
enum Enum
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
    Count
};

static const char* Strings[] = { "Repeat", "MirroredRepeat", "ClampToEdge", "ClampToBorder", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace TextureMipMapMode {
enum Enum
{
    Nearest,
    Linear,
    Count
};

static const char* Strings[] = { "Nearest", "Linear", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace SharingMode {
enum Enum
{
    Exclusive,
    Concurrent,
    Count
};

static const char* Strings[] = { "Exclusive", "Concurrent", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

enum CullModeFlags
{
    CULL_MODE_FLAGS_NONE = 1 << 0,
    CULL_MODE_FLAGS_FRONT = 1 << 1,
    CULL_MODE_FLAGS_BACK = 1 << 2,
    CULL_MODE_FLAGS_FRONT_AND_BACK = 1 << 3,
};

enum TextureUsageFlags
{
    TEXTURE_USAGE_FLAGS_TRANSFER_SRC = 1 << 0,
    TEXTURE_USAGE_FLAGS_TRANSFER_DST = 1 << 1,
    TEXTURE_USAGE_FLAGS_SAMPLED = 1 << 2,
    TEXTURE_USAGE_FLAGS_STORAGE = 1 << 3,
    TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT = 1 << 4,
    TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT = 1 << 5,
};

enum TextureSampleCountFlags
{
    TEXTURE_SAMPLE_COUNT_FLAGS_1 = 1 << 0,
    TEXTURE_SAMPLE_COUNT_FLAGS_2 = 1 << 1,
    TEXTURE_SAMPLE_COUNT_FLAGS_4 = 1 << 2,
    TEXTURE_SAMPLE_COUNT_FLAGS_8 = 1 << 3,
    TEXTURE_SAMPLE_COUNT_FLAGS_16 = 1 << 4,
    TEXTURE_SAMPLE_COUNT_FLAGS_32 = 1 << 5,
    TEXTURE_SAMPLE_COUNT_FLAGS_64 = 1 << 6,
};

enum BufferUsageFlags
{
    BUFFER_USAGE_FLAGS_TRANSFER_SRC = 1 << 0,
    BUFFER_USAGE_FLAGS_TRANSFER_DST = 1 << 1,
    BUFFER_USAGE_FLAGS_UNIFORM_TEXEL_BUFFER = 1 << 2,
    BUFFER_USAGE_FLAGS_STORAGE_TEXEL_BUFFER = 1 << 3,
    BUFFER_USAGE_FLAGS_UNIFORM_BUFFER = 1 << 4,
    BUFFER_USAGE_FLAGS_STORAGE_BUFFER = 1 << 5,
    BUFFER_USAGE_FLAGS_INDEX_BUFFER = 1 << 6,
    BUFFER_USAGE_FLAGS_VERTEX_BUFFER = 1 << 7,
    BUFFER_USAGE_FLAGS_INDIRECT_BUFFER = 1 << 8,
};

enum MemoryPropertyFlags
{
    MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL = 1 << 0,
    MEMORY_PROPERTY_FLAGS_HOST_VISIBLE = 1 << 1,
    MEMORY_PROPERTY_FLAGS_HOST_COHERENT = 1 << 2,
    MEMORY_PROPERTY_FLAGS_HOST_CACHED = 1 << 3,
};

enum ShaderStageFlags
{
    SHADER_STAGE_FLAGS_VERTEX = 1 << 0,
    SHADER_STAGE_FLAGS_GEOMETRY = 1 << 1,
    SHADER_STAGE_FLAGS_FRAGMENT = 1 << 2,
    SHADER_STAGE_FLAGS_COMPUTE = 1 << 3,
};

namespace LoadOp {
enum Enum
{
    Load,
    Clear,
    DontCare,
    Count
};
static const char* Strings[] = { "Load", "Clear", "DontCare" };
static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace StoreOp {
enum Enum
{
    Store,
    DontCare,
    Count
};
static const char* Strings[] = { "Store", "DontCare" };
static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

namespace TextureLayout {
enum Enum
{
    Undefined,
    RenderTarget,
    DepthStencil,
    PresentationTarget,
    Sampled,
    Count
};

static const char* Strings[] = { "Undefined", "RenderTarget", "DepthStencil", "PresentationTarget", "Sampled" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
};

struct DepthTarget
{
    Handle<Texture>     Texture;
    LoadOp::Enum        LoadOperation = LoadOp::Clear;
    StoreOp::Enum       StoreOperation = StoreOp::Store;
    LoadOp::Enum        StencilLoadOperation = LoadOp::Clear;
    StoreOp::Enum       StencilStoreOperation = StoreOp::Store;
    TextureLayout::Enum NextUsage = TextureLayout::DepthStencil;
    float32             Depth = 1.0f;
    uint32              Stencil = 0;
};

struct ColorTarget
{
    Handle<Texture>     Texture;
    LoadOp::Enum        LoadOperation = LoadOp::Clear;
    StoreOp::Enum       StoreOperation = StoreOp::Store;
    TextureLayout::Enum NextUsage = TextureLayout::RenderTarget;
    Vec4f               ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct TextureSamplerDescription
{
    TextureFilter::Enum     MinFilter = TextureFilter::Linear;
    TextureFilter::Enum     MagFilter = TextureFilter::Linear;
    TextureWrapMode::Enum   WrapModeU = TextureWrapMode::Repeat;
    TextureWrapMode::Enum   WrapModeV = TextureWrapMode::Repeat;
    TextureWrapMode::Enum   WrapModeW = TextureWrapMode::Repeat;
    TextureMipMapMode::Enum MipMapMode = TextureMipMapMode::Linear;
    CompareOp::Enum         Compare = CompareOp::Never;
    bool                    AnisotropyEnabled = false;
};

struct TextureDescription
{
    const char* DebugName = "";

    Vec4f                   Dimensions;
    Format::Enum            Format;
    uint64                  Usage;
    TextureTiling::Enum     Tiling = TextureTiling::Optimal;
    TextureType::Enum       Type = TextureType::Type2D;
    TextureSampleCountFlags SampleCount = TEXTURE_SAMPLE_COUNT_FLAGS_1;
    uint32                  MipLevels = 1;
    SharingMode::Enum       SharingMode = SharingMode::Exclusive;

    // Sampler description
    bool                      CreateSampler = true;
    TextureSamplerDescription Sampler;
};

struct BufferDescription
{
    uint64            Size;
    uint64            UsageFlags;
    uint64            MemoryPropertyFlags;
    SharingMode::Enum SharingMode = SharingMode::Exclusive;
    bool              BindMemory = true;
    bool              MapMemory = false;
};

struct Buffer
{
    uint64 Size = 0;
    void*  Ptr = nullptr;

    // TODO (amn): Figure out what other fields we need here
};

struct RenderPassSubpass
{
    bool         DepthTarget = false;
    Array<uint8> ColorTargetSlots;
};

struct RenderPassLayout
{
    Array<RenderPassSubpass> Subpasses;
};

struct RenderPassDescription
{
    const char* DebugName;

    Vec4f              Dimensions;
    Array<ColorTarget> ColorTargets;
    DepthTarget        DepthTarget;
    RenderPassLayout   Layout;
};

struct RenderPass
{
    Vec4f              Dimensions;
    DepthTarget        DepthTarget;
    Array<ColorTarget> ColorTargets;
};

struct UploadBufferDescription
{
    Handle<Buffer> DstBuffer;
    Handle<Buffer> SrcBuffer;
    uint64         SrcSize;
    uint64         DstOffset = 0;
    uint64         SrcOffset = 0;
};

struct PhysicalDeviceFormatSpecs
{
    Format::Enum SwapchainFormat;
    Format::Enum DepthBufferFormat;
};

} // namespace::renderer

} // namespace::kraft