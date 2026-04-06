#pragma once

#include <core/kraft_core.h>

namespace kraft {
struct Texture;
struct TextureSampler;
} // namespace kraft

namespace kraft::r {

template <typename T> struct Handle;
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

struct ResourceManager {
    void (*Clear)();

    // Creation apis
    Handle<Texture> (*CreateTexture)(const TextureDescription& description) = 0;
    Handle<TextureSampler> (*CreateTextureSampler)(const TextureSamplerDescription& description) = 0;
    Handle<Buffer> (*CreateBuffer)(const BufferDescription& description) = 0;
    Handle<RenderPass> (*CreateRenderPass)(const RenderPassDescription& description) = 0;
    Handle<CommandBuffer> (*CreateCommandBuffer)(const CommandBufferDescription& description) = 0;
    Handle<CommandPool> (*CreateCommandPool)(const CommandPoolDescription& description) = 0;
    Handle<BindGroup> (*CreateBindGroup)(const BindGroupDescription& description) = 0;
    // virtual Handle<ShaderEffect> CreateShaderEffect(ShaderEffectDescription description) = 0;

    // Destruction apis
    void (*DestroyTexture)(Handle<Texture> handle) = 0;
    // We don't want to allow sampler deletion
    // void (*DestroyTextureSampler)(Handle<TextureSampler> handle) = 0;
    void (*DestroyBuffer)(Handle<Buffer> handle) = 0;
    void (*DestroyRenderPass)(Handle<RenderPass> handle) = 0;
    void (*DestroyCommandBuffer)(Handle<CommandBuffer> handle);
    void (*DestroyCommandPool)(Handle<CommandPool> handle) = 0;

    // Access apis
    u8* (*GetBufferData)(Handle<Buffer> buffer) = 0;
    u64 (*GetBufferDeviceAddress)(Handle<Buffer> buffer) = 0;

    // Creates a temporary buffer that gets destroyed at the end of the frame
    BufferView (*CreateTempBuffer)(u64 size) = 0;

    // Uploads data from the the `Buffer` to the `Texture`
    bool (*UploadTexture)(Handle<Texture> texture, Handle<Buffer> buffer, u64 buffer_offset) = 0;
    // Uploads raw buffer data to the GPU
    bool (*UploadBuffer)(const UploadBufferDescription& description) = 0;
    bool (*ReadTextureData)(const ReadTextureDataDescription& description) = 0;

    Texture* (*GetTextureMetadata)(Handle<Texture> handle) = 0;
    RenderPass* (*GetRenderPassMetadata)(Handle<RenderPass> handle) = 0;
    void (*StartFrame)(u64 frame_number) = 0;
    void (*EndFrame)(u64 frame_number) = 0;

    void SetPhysicalDeviceFormatSpecs(const PhysicalDeviceFormatSpecs& specifications);
    const PhysicalDeviceFormatSpecs& GetPhysicalDeviceFormatSpecs() const;
};

extern struct ResourceManager* ResourceManager;

}; // namespace kraft::r