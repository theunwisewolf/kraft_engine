#pragma once

#include <renderer/kraft_resource_manager.h>

namespace kraft {
struct Texture;
}

namespace kraft::renderer {

template<typename ConcreteType, typename Type>
struct Pool;

template<typename T>
struct Handle;

struct Buffer;
struct RenderPass;
struct CommandBuffer;
struct CommandPool;
struct VulkanTexture;
struct VulkanBuffer;
struct VulkanRenderPass;
struct VulkanCommandBuffer;
struct VulkanCommandPool;

class VulkanTempMemoryBlockAllocator : public TempMemoryBlockAllocator
{
public:
    VulkanTempMemoryBlockAllocator(uint64 BlockSize);
    GPUBuffer GetNextFreeBlock() final;
    ~VulkanTempMemoryBlockAllocator() {};
};

class VulkanResourceManager : public ResourceManager
{
public:
    VulkanResourceManager();
    virtual ~VulkanResourceManager() {}

    // Destroys all allocated GPU resources
    void Clear();

    virtual Handle<Texture>    CreateTexture(const TextureDescription& Description) final override;
    virtual Handle<Buffer>     CreateBuffer(const BufferDescription& Description) final override;
    virtual Handle<RenderPass> CreateRenderPass(const RenderPassDescription& Description) final override;
    Handle<CommandBuffer>      CreateCommandBuffer(const CommandBufferDescription& Description) final override;
    Handle<CommandPool>        CreateCommandPool(const CommandPoolDescription& Description) final override;

    virtual void DestroyTexture(Handle<Texture> Resource) final override;
    virtual void DestroyBuffer(Handle<Buffer> Resource) final override;
    virtual void DestroyRenderPass(Handle<RenderPass> Resource) final override;
    void         DestroyCommandBuffer(Handle<CommandBuffer> Resource) final override;
    void         DestroyCommandPool(Handle<CommandPool> Resource) final override;

    virtual uint8* GetBufferData(Handle<Buffer> Buffer) final override;

    // Creates a temporary buffer that gets destroyed at the end of the frame
    virtual GPUBuffer CreateTempBuffer(uint64 Size) final override;

    // Uploads a raw buffer data to the GPU
    virtual bool UploadTexture(Handle<Texture> TextureResource, Handle<Buffer> BufferResource, uint64 BufferOffset) final override;
    virtual bool UploadBuffer(const UploadBufferDescription& Description) final override;
    bool ReadTextureData(const ReadTextureDataDescription& Description) final override;

    virtual Texture*                                GetTextureMetadata(Handle<Texture> Resource) override;
    const Pool<VulkanTexture, Texture>&             GetTexturePool() const;
    const Pool<VulkanBuffer, Buffer>&               GetBufferPool() const;
    const Pool<VulkanRenderPass, RenderPass>&       GetRenderPassPool() const;
    const Pool<VulkanCommandBuffer, CommandBuffer>& GetCommandBufferPool() const;
    const Pool<VulkanCommandPool, CommandPool>&     GetCommandPoolPool() const;

    virtual void StartFrame(uint64 FrameNumber) override;
    virtual void EndFrame(uint64 FrameNumber) override;
};

}