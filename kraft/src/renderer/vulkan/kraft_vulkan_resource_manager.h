#pragma once

#include <renderer/kraft_resource_manager.h>

namespace kraft {
struct Texture;
struct ArenaAllocator;
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

struct ResourceManagerState;

struct VulkanResourceManagerApi
{
    static VulkanTexture*       GetTexture(const ResourceManagerState* State, Handle<Texture> Resource);
    static VulkanBuffer*        GetBuffer(const ResourceManagerState* State, Handle<Buffer> Resource);
    static VulkanRenderPass*    GetRenderPass(const ResourceManagerState* State, Handle<RenderPass> Resource);
    static VulkanCommandBuffer* GetCommandBuffer(const ResourceManagerState* State, Handle<CommandBuffer> Resource);
    static VulkanCommandPool*   GetCommandPool(const ResourceManagerState* State, Handle<CommandPool> Resource);

    static void Destroy();
};

struct ResourceManager* CreateVulkanResourceManager(ArenaAllocator* Arena);
void                    DestroyVulkanResourceManager(struct ResourceManager* ResourceManager);

}