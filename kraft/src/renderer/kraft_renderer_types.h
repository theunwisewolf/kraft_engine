#pragma once

struct ImDrawData;

namespace kraft {

struct EngineConfigT;
struct Texture;
struct Material;
struct Geometry;
struct Shader;
struct ShaderUniform;
struct World;

namespace renderer {

struct RenderPass;
struct CommandBuffer;
struct Buffer;

// Templated only for type-safety
template<typename T>
struct Handle
{
private:
    uint16 Index;
    uint16 Generation;

    template<typename U, typename V>
    friend struct Pool;
    Handle(uint16 Index, uint16 Generation) : Index(Index), Generation(Generation)
    {}

public:
    Handle() : Index(0), Generation(0xffff)
    {}

    static Handle Invalid()
    {
        return Handle(0, 0xffff);
    }

    bool IsInvalid() const
    {
        return this->Generation == 0xffff;
    }

    explicit operator bool() const
    {
        return this->Generation != 0xffff;
    }

    bool operator==(const Handle<T> Other)
    {
        return Other.Generation == Generation && Other.Index == Index;
    }

    bool operator!=(const Handle<T> Other)
    {
        return Other.Generation != Generation || Other.Index != Index;
    }
};

struct ShaderEffect;

struct alignas(16) GlobalShaderData
{
    union
    {
        struct alignas(16)
        {
            Mat4f Projection;
            Mat4f View;
            Vec3f GlobalLightPosition;
            Vec4f GlobalLightColor = kraft::Vec4fOne;
            Vec3f CameraPosition;
        };

        char _[256];
    };
};

struct Renderable
{
    kraft::Mat4f ModelMatrix;
    Material*    MaterialInstance;
    uint32       GeometryId;
    uint32       EntityId;
};

struct RenderPacket
{
    Mat4f ProjectionMatrix;
    Mat4f ViewMatrix;
};

enum RendererBackendType : int
{
    RENDERER_BACKEND_TYPE_NONE,
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,

    RENDERER_BACKEND_TYPE_NUM_COUNT
};

struct DeviceInfoT
{};

struct RendererBackend
{
    bool (*Init)(EngineConfigT* Config);
    bool (*Shutdown)();
    int (*PrepareFrame)();
    bool (*BeginFrame)();
    bool (*EndFrame)();
    void (*OnResize)(int width, int height);

    // Shader
    void (*UseShader)(const Shader* Shader);
    void (*SetUniform)(Shader* Shader, const ShaderUniform& Uniform, void* Value, bool Invalidate);
    void (*ApplyGlobalShaderProperties)(Shader* ActiveShader, Handle<Buffer> GlobalUBOBuffer);
    void (*ApplyInstanceShaderProperties)(Shader* ActiveShader);
    void (*CreateRenderPipeline)(Shader* Shader, int PassIndex, Handle<RenderPass> RenderPassHandle);
    void (*DestroyRenderPipeline)(Shader* Shader);
    void (*CreateMaterial)(Material* Material);
    void (*DestroyMaterial)(Material* Material);

    // Geometry
    void (*DrawGeometryData)(uint32 GeometryID);
    bool (*CreateGeometry)(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize);
    void (*DestroyGeometry)(Geometry* Geometry);

    // Render Passes
    void (*BeginRenderPass)(Handle<CommandBuffer> CmdBufferHandle, Handle<RenderPass> PassHandle);
    void (*EndRenderPass)(Handle<CommandBuffer> CmdBufferHandle, Handle<RenderPass> PassHandle);

    // Misc
    bool (*ReadObjectPickingBuffer)(uint32** OutBuffer, uint32* BufferSize);

    DeviceInfoT DeviceInfo;
};

struct Vertex2D
{
    Vec3f Position;
    Vec2f UV;
};

struct Vertex3D
{
    Vec3f Position;
    Vec2f UV;
    Vec3f Normal;
};

namespace ShaderDataType {
enum Enum : int
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

static const char* Strings[] = { "Float", "Float2", "Float3", "Float4", "Mat4", "Byte", "Byte4N", "UByte", "UByte4N", "Short2", "Short2N", "Short4", "Short4N", "UInt", "UInt2", "UInt4", "Count" };

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
    StorageBuffer,
    ConstantBuffer,
    Count
};

static const char* Strings[] = { "Sampler", "UniformBuffer", "StorageBuffer", "ConstantBuffer", "Count" };

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
enum Enum : int
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
    "Zero", "One", "SrcColor", "OneMinusSrcColor", "DstColor", "OneMinusDstColor", "SrcAlpha", "OneMinusSrcAlpha", "DstAlpha", "OneMinusDstAlpha", "Count",
};

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

namespace BlendOp {
enum Enum : int
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
    R32_SFLOAT,
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

static const char* Strings[] = { "RED",       "R32_SFLOAT", "RGBA8_UNORM",       "RGB8_UNORM",        "BGRA8_UNORM",        "BGR8_UNORM",
                                 "D16_UNORM", "D32_SFLOAT", "D16_UNORM_S8_UINT", "D24_UNORM_S8_UINT", "D32_SFLOAT_S8_UINT", "Count" };

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

namespace CullModeFlags {
enum Enum
{
    None,
    Front,
    Back,
    FrontAndBack = Front | Back,
    Count,
};

static const char* Strings[] = { "None", "Front", "Back", "BackAndFront", "Count" };

static const char* String(Enum Value)
{
    return (Value < Enum::Count ? Strings[(int)Value] : "Unsupported");
}
}

enum TextureUsageFlags
{
    TEXTURE_USAGE_FLAGS_TRANSFER_SRC = 1 << 0,
    TEXTURE_USAGE_FLAGS_TRANSFER_DST = 1 << 1,
    TEXTURE_USAGE_FLAGS_SAMPLED = 1 << 2,
    TEXTURE_USAGE_FLAGS_STORAGE = 1 << 3,
    TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT = 1 << 4,
    TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT = 1 << 5,
    TEXTURE_USAGE_FLAGS_TRANSIENT_ATTACHMENT = 1 << 6,
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

enum ShaderStageFlags : int
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
    const char* DebugName;

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

struct GPUBuffer
{
    Handle<Buffer> GPUBuffer = {};
    uint8*         Ptr = 0;    // Location of data inside the GPUBuffer
    uint64         Offset = 0; // Offset from the start of the buffer
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

enum CommandPoolCreateFlags : int32
{
    COMMAND_POOL_CREATE_FLAGS_TRANSIENT_BIT = 0x00000001,
    COMMAND_POOL_CREATE_FLAGS_RESET_COMMAND_BUFFER_BIT = 0x00000002,
    COMMAND_POOL_CREATE_FLAGS_PROTECTED_BIT = 0x00000004,
};

struct CommandPool
{};

struct CommandPoolDescription
{
    const char* DebugName;

    uint32                 QueueFamilyIndex;
    CommandPoolCreateFlags Flags;
};

struct CommandBuffer
{
    /// The GPU command pool this CmdBuffer was allocated from
    Handle<CommandPool> Pool;
};

struct CommandBufferDescription
{
    const char* DebugName;

    /// The GPU command pool to allocate this CmdBuffer from
    Handle<CommandPool> CommandPool;

    /// false if the buffer is supposed to be a secondary command buffer
    bool Primary = true;

    /// true if you want to begin the buffer right away
    bool Begin = false;

    /// Is this a single-use command buffer?
    /// Only valid if `CommandBufferDescription.Begin` is true
    bool SingleUse = false;

    /// Only valid if `CommandBufferDescription.Begin` is true
    bool RenderPassContinue = false;

    /// Only valid if `CommandBufferDescription.Begin` is true
    bool SimultaneousUse = false;
};

struct UploadBufferDescription
{
    Handle<Buffer> DstBuffer;
    Handle<Buffer> SrcBuffer;
    uint64         SrcSize;
    uint64         DstOffset = 0;
    uint64         SrcOffset = 0;
};

struct ReadTextureDataDescription
{
    Handle<Texture>       SrcTexture = Handle<Texture>::Invalid();
    void*                 OutBuffer = nullptr;
    uint32                OutBufferSize = 0;
    Handle<CommandBuffer> CmdBuffer = Handle<CommandBuffer>::Invalid();
    int32                 OffsetX = 0;
    int32                 OffsetY = 0;
    uint32                Width = 0;  // if 0, the entire image will be copied to the buffer
    uint32                Height = 0; // if 0, the entire image will be copied to the buffer
};

struct PhysicalDeviceFormatSpecs
{
    Format::Enum SwapchainFormat;
    Format::Enum DepthBufferFormat;
};

struct RenderSurfaceT
{
    const char* DebugName;

    uint32                Width;
    uint32                Height;
    Handle<CommandBuffer> CmdBuffers[3];
    Handle<RenderPass>    RenderPass;
    Handle<Texture>       ColorPassTexture;
    Handle<Texture>       DepthPassTexture;
    Handle<Buffer>        GlobalUBO;
    World*                World;
    kraft::Vec2f          RelativeMousePosition;

    void Begin();
    void End();
};

} // namespace::renderer

} // namespace::kraft