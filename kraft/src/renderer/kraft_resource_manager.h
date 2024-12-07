#pragma once

#include <core/kraft_core.h>

namespace kraft {
struct Texture;
}

namespace kraft::renderer {

template<typename T>
struct Handle;
struct Buffer;
struct ShaderEffect;
struct UploadBufferDescription;
struct TextureDescription;
struct BufferDescription;
struct RenderPassDescription;
struct RenderPass;
struct PhysicalDeviceFormatSpecs;
struct GPUBuffer;

class TempMemoryBlockAllocator
{
public:
    uint64            BlockSize;
    uint64            AllocationCount = 0;
    virtual GPUBuffer GetNextFreeBlock() = 0;
    virtual ~TempMemoryBlockAllocator() {};
};

struct TempAllocator
{
    uint64                    CurrentOffset;
    TempMemoryBlockAllocator* Allocator;

    GPUBuffer Allocate(uint64 Size, uint64 Alignment);
};

class ResourceManager
{
protected:
    TempAllocator TempGPUAllocator;

public:
    static ResourceManager* Ptr;

    KRAFT_INLINE static ResourceManager* Get()
    {
        return Ptr;
    }

    ResourceManager();
    virtual ~ResourceManager() = 0;

    // Creation apis
    virtual Handle<Texture>    CreateTexture(const TextureDescription& Description) = 0;
    virtual Handle<Buffer>     CreateBuffer(const BufferDescription& Description) = 0;
    virtual Handle<RenderPass> CreateRenderPass(const RenderPassDescription& Description) = 0;
    // virtual Handle<ShaderEffect> CreateShaderEffect(ShaderEffectDescription Description) = 0;

    // Destruction apis
    virtual void DestroyTexture(Handle<Texture> Resource) = 0;
    virtual void DestroyBuffer(Handle<Buffer> Resource) = 0;
    virtual void DestroyRenderPass(Handle<RenderPass> Resource) = 0;

    // Access apis
    virtual uint8* GetBufferData(Handle<Buffer> Buffer) = 0;

    // Creates a temporary buffer that gets destroyed at the end of the frame
    virtual GPUBuffer CreateTempBuffer(uint64 Size) = 0;

    // Uploads a raw buffer data to the GPU
    virtual bool UploadTexture(Handle<Texture> Texture, Handle<Buffer> Buffer, uint64 BufferOffset) = 0;
    virtual bool UploadBuffer(const UploadBufferDescription& Description) = 0;

    virtual Texture* GetTextureMetadata(Handle<Texture> Resource) = 0;

    virtual void StartFrame(uint64 FrameNumber) = 0;
    virtual void EndFrame(uint64 FrameNumber) = 0;

    void                             SetPhysicalDeviceFormatSpecs(const PhysicalDeviceFormatSpecs& Specs);
    const PhysicalDeviceFormatSpecs& GetPhysicalDeviceFormatSpecs() const;
};

};