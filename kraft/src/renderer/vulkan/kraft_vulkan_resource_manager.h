#pragma once

#include <renderer/kraft_resource_manager.h>

namespace kraft {
struct Texture;
struct ArenaAllocator;
} // namespace kraft

namespace kraft::renderer {

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

} // namespace kraft::renderer