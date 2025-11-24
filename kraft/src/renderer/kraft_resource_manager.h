#pragma once

#include <core/kraft_core.h>

namespace kraft {
struct Texture;
struct TextureSampler;
} // namespace kraft

namespace kraft::r {

template<typename T>
struct Handle;
struct Buffer;
struct ShaderEffect;
struct RenderPass;
struct CommandBuffer;
struct CommandPool;
struct PhysicalDeviceFormatSpecs;
struct BufferView;
struct UploadBufferDescription;
struct ReadTextureDataDescription;
struct TextureDescription;
struct TextureSamplerDescription;
struct BufferDescription;
struct RenderPassDescription;
struct CommandBufferDescription;
struct CommandPoolDescription;

struct ResourceManager
{
    void (*Clear)();

    // Creation apis
    Handle<Texture> (*CreateTexture)(const TextureDescription& Description) = 0;
    Handle<TextureSampler> (*CreateTextureSampler)(const TextureSamplerDescription& Description) = 0;
    Handle<Buffer> (*CreateBuffer)(const BufferDescription& Description) = 0;
    Handle<RenderPass> (*CreateRenderPass)(const RenderPassDescription& Description) = 0;
    Handle<CommandBuffer> (*CreateCommandBuffer)(const CommandBufferDescription& Description) = 0;
    Handle<CommandPool> (*CreateCommandPool)(const CommandPoolDescription& Description) = 0;
    // virtual Handle<ShaderEffect> CreateShaderEffect(ShaderEffectDescription Description) = 0;

    // Destruction apis
    void (*DestroyTexture)(Handle<Texture> Resource) = 0;
    void (*DestroyTextureSampler)(Handle<TextureSampler> Resource) = 0;
    void (*DestroyBuffer)(Handle<Buffer> Resource) = 0;
    void (*DestroyRenderPass)(Handle<RenderPass> Resource) = 0;
    void (*DestroyCommandBuffer)(Handle<CommandBuffer> Resource);
    void (*DestroyCommandPool)(Handle<CommandPool> Resource) = 0;

    // Access apis
    uint8* (*GetBufferData)(Handle<Buffer> Buffer) = 0;

    // Creates a temporary buffer that gets destroyed at the end of the frame
    BufferView (*CreateTempBuffer)(uint64 Size) = 0;

    // Uploads data from the the `Buffer` to the `Texture`
    bool (*UploadTexture)(Handle<Texture> Texture, Handle<Buffer> Buffer, uint64 BufferOffset) = 0;
    // Uploads raw buffer data to the GPU
    bool (*UploadBuffer)(const UploadBufferDescription& Description) = 0;
    bool (*ReadTextureData)(const ReadTextureDataDescription& Description) = 0;

    Texture* (*GetTextureMetadata)(Handle<Texture> Resource) = 0;
    RenderPass* (*GetRenderPassMetadata)(Handle<RenderPass> Resource) = 0;
    void (*StartFrame)(uint64 FrameNumber) = 0;
    void (*EndFrame)(uint64 FrameNumber) = 0;

    void                             SetPhysicalDeviceFormatSpecs(const PhysicalDeviceFormatSpecs& Specs);
    const PhysicalDeviceFormatSpecs& GetPhysicalDeviceFormatSpecs() const;
};

extern struct ResourceManager* ResourceManager;

}; // namespace kraft::renderer