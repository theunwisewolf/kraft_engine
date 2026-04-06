#pragma once

namespace kraft {
struct Texture;
struct ArenaAllocator;
} // namespace kraft

namespace kraft::r {

template <typename T> struct Handle;

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

struct TempMemoryBlock {
    BufferView block;
    TempMemoryBlock* next;
    TempMemoryBlock* prev;
};

struct VulkanTempMemoryBlockAllocator {
    u64 current_free_offset = u64(-1); // Offset of the memory that is current free
    u64 block_size = 0; // Size of each "block" to allocate
    u64 allocation_count = 0; // Number of allocations
    TempMemoryBlock* current_block = nullptr; // Current Memory block

    void Initialize(ArenaAllocator* arena, u64 block_size);
    void Destroy();
    void Clear();

    BufferView Allocate(ArenaAllocator* arena, u64 size, u64 alignment);
    BufferView AllocateGPUMemory(ArenaAllocator* arena);
};

struct VulkanResourceManagerState {
    ArenaAllocator* arena;
    Pool<VulkanTexture, Texture> texture_pool;
    Pool<VulkanTextureSampler, TextureSampler> texture_sampler_pool;
    Pool<VulkanBuffer, Buffer> buffer_pool;
    Pool<VulkanRenderPass, RenderPass> render_pass_pool;
    Pool<VulkanCommandBuffer, CommandBuffer> cmd_buffer_pool;
    Pool<VulkanCommandPool, CommandPool> cmd_pool_pool;
    Pool<VulkanBindGroup, BindGroup> bind_group_pool;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VulkanTempMemoryBlockAllocator* temp_gpu_allocator = nullptr;
};

struct VulkanResourceManagerApi {
    static VulkanTexture* GetTexture(Handle<Texture> handle);
    static VulkanTextureSampler* GetTextureSampler(Handle<TextureSampler> handle);
    static VulkanBuffer* GetBuffer(Handle<Buffer> handle);
    static VulkanRenderPass* GetRenderPass(Handle<RenderPass> handle);
    static VulkanCommandBuffer* GetCommandBuffer(Handle<CommandBuffer> handle);
    static VulkanCommandPool* GetCommandPool(Handle<CommandPool> handle);
    static VulkanBindGroup* GetBindGroup(Handle<BindGroup> handle);
    static BindGroup* GetBindGroupMetadata(Handle<BindGroup> handle);

    // Returns the number of active samplers and fills out_samplers with their VkSampler handles
    static u32 GetActiveSamplers(VkSampler* out_samplers, u32 max_count);

    // Returns the VkDeviceAddress for a buffer created with BUFFER_USAGE_FLAGS_SHADER_DEVICE_ADDRESS
    static VkDeviceAddress GetBufferDeviceAddress(Handle<Buffer> handle);
};

struct ResourceManager* CreateVulkanResourceManager(ArenaAllocator* arena);
void DestroyVulkanResourceManager(struct ResourceManager* resource_manager);

} // namespace kraft::r