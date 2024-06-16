#pragma once

#include <renderer/kraft_resource_manager.h>
#include <renderer/vulkan/kraft_vulkan_types.h>

namespace kraft::renderer
{

class VulkanTempMemoryBlockAllocator : public TempMemoryBlockAllocator
{
public:
    VulkanTempMemoryBlockAllocator(uint64 BlockSize);
    Block GetNextFreeBlock() final;
};

class VulkanResourceManager : public ResourceManager
{
public:
    VulkanResourceManager();
    virtual ~VulkanResourceManager();
    
    virtual Handle<Texture> CreateTexture(TextureDescription Description) final;
    virtual Handle<Buffer> CreateBuffer(BufferDescription Description) final;
    virtual Handle<RenderPass> CreateRenderPass(RenderPassDescription Description) final;

    virtual void DestroyTexture(Handle<Texture> Resource) final;
    virtual void DestroyBuffer(Handle<Buffer> Resource) final;
    virtual void DestroyRenderPass(Handle<RenderPass> Resource) final;
    virtual uint8* GetBufferData(Handle<Buffer> Buffer) final;

    // Creates a temporary buffer that gets destroyed at the end of the frame
    virtual TempBuffer CreateTempBuffer(uint64 Size) final;

    // Uploads a raw buffer data to the GPU
    virtual bool UploadTexture(Handle<Texture> TextureResource, Handle<Buffer> BufferResource, uint64 BufferOffset) final;
    virtual bool UploadBuffer(UploadBufferDescription Description) final;
    
    virtual Texture* GetTextureMetadata(Handle<Texture> Resource);
    KRAFT_INLINE const Pool<VulkanTexture, Texture>& GetTexturePool() const { return TexturePool; }
    KRAFT_INLINE const Pool<VulkanBuffer, Buffer>& GetBufferPool() const { return BufferPool; }
    KRAFT_INLINE const Pool<VulkanRenderPass, RenderPass>& GetRenderPassPool() const { return RenderPassPool; }

    virtual void StartFrame(uint64 FrameNumber);
    virtual void EndFrame(uint64 FrameNumber);

private:
    Pool<VulkanTexture, Texture> TexturePool;   
    Pool<VulkanBuffer, Buffer> BufferPool;
    Pool<VulkanRenderPass, RenderPass> RenderPassPool;
};

}