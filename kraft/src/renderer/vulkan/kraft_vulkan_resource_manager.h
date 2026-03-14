#pragma once

namespace kraft {
struct Texture;
struct ArenaAllocator;
} // namespace kraft

namespace kraft::r {

template<typename T>
struct Handle;

struct Buffer;
struct RenderPass;
struct CommandBuffer;
struct CommandPool;
struct VulkanTexture;
struct VulkanTextureSampler;
struct VulkanBuffer;
struct VulkanRenderPass;
struct VulkanCommandBuffer;
struct VulkanCommandPool;

struct TempMemoryBlock
{
    BufferView       Block;
    TempMemoryBlock* Next;
    TempMemoryBlock* Prev;
};

struct VulkanTempMemoryBlockAllocator
{
    u64              CurrentFreeOffset = u64(-1); // Offset of the memory that is current free
    u64              BlockSize = 0;               // Size of each "block" to allocate
    u64              AllocationCount = 0;         // Number of allocations
    TempMemoryBlock* CurrentBlock = nullptr;      // Current Memory block

    void Initialize(ArenaAllocator* Arena, u64 BlockSize);
    void Destroy();
    void Clear();

    BufferView Allocate(ArenaAllocator* Arena, u64 Size, u64 Alignment);
    BufferView AllocateGPUMemory(ArenaAllocator* Arena);
};

struct VulkanResourceManagerState
{
    ArenaAllocator*                            Arena;
    Pool<VulkanTexture, Texture>               TexturePool;
    Pool<VulkanTextureSampler, TextureSampler> TextureSamplerPool;
    Pool<VulkanBuffer, Buffer>                 BufferPool;
    Pool<VulkanRenderPass, RenderPass>         RenderPassPool;
    Pool<VulkanCommandBuffer, CommandBuffer>   CmdBufferPool;
    Pool<VulkanCommandPool, CommandPool>       CmdPoolPool;
    VulkanTempMemoryBlockAllocator*            TempGPUAllocator = nullptr;
};

struct VulkanResourceManagerApi
{
    static VulkanTexture*        GetTexture(Handle<Texture> Resource);
    static VulkanTextureSampler* GetTextureSampler(Handle<TextureSampler> Resource);
    static VulkanBuffer*         GetBuffer(Handle<Buffer> Resource);
    static VulkanRenderPass*     GetRenderPass(Handle<RenderPass> Resource);
    static VulkanCommandBuffer*  GetCommandBuffer(Handle<CommandBuffer> Resource);
    static VulkanCommandPool*    GetCommandPool(Handle<CommandPool> Resource);
};

struct ResourceManager* CreateVulkanResourceManager(ArenaAllocator* Arena);
void                    DestroyVulkanResourceManager(struct ResourceManager* ResourceManager);

} // namespace kraft::r