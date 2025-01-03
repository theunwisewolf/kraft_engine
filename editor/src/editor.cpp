#include "editor.h"

#include "world/kraft_entity.h"
#include <core/kraft_engine.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/kraft_resource_manager.h>
#include <systems/kraft_shader_system.h>

EditorState* EditorState::Ptr = nullptr;

EditorState::EditorState()
{
    EditorState::Ptr = this;
    this->RenderSurface = kraft::Engine::Renderer.CreateRenderSurface("SceneView", 200.0f, 200.0f, true);

    const uint32                    Width = 200;
    const uint32                    Height = 200;
    kraft::renderer::RenderSurfaceT Surface = {
        .DebugName = "ObjectPickingSurface",
        .Width = Width,
        .Height = Height,
    };

    Surface.ColorPassTexture = kraft::renderer::ResourceManager::Ptr->CreateTexture({
        .DebugName = "ObjectPickingCT",
        .Dimensions = { (float32)Width, (float32)Height, 1, 4 },
        .Format = kraft::renderer::Format::BGRA8_UNORM,
        .Usage = kraft::renderer::TextureUsageFlags::TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT | kraft::renderer::TextureUsageFlags::TEXTURE_USAGE_FLAGS_TRANSFER_SRC,
        .Sampler = {
            .WrapModeU = kraft::renderer::TextureWrapMode::ClampToEdge,
            .WrapModeV = kraft::renderer::TextureWrapMode::ClampToEdge,
            .WrapModeW = kraft::renderer::TextureWrapMode::ClampToEdge,
            .Compare = kraft::renderer::CompareOp::Always,
        },
    });

    Surface.DepthPassTexture = kraft::renderer::ResourceManager::Ptr->CreateTexture({
        .DebugName = "ObjectPickingDT",
        .Dimensions = { (float32)Width, (float32)Height, 1, 4 },
        .Format = kraft::renderer::ResourceManager::Ptr->GetPhysicalDeviceFormatSpecs().DepthBufferFormat,
        .Usage = kraft::renderer::TextureUsageFlags::TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT,
        .Sampler = {
            .WrapModeU = kraft::renderer::TextureWrapMode::ClampToEdge,
            .WrapModeV = kraft::renderer::TextureWrapMode::ClampToEdge,
            .WrapModeW = kraft::renderer::TextureWrapMode::ClampToEdge,
            .Compare = kraft::renderer::CompareOp::Always,
        },
    });

    Surface.RenderPass = kraft::renderer::ResourceManager::Ptr->CreateRenderPass({
        .DebugName = "ObjectPickingRP",
        .Dimensions = { (float)Width, (float)Height, 0.f, 0.f },
        .ColorTargets = {
            {
                .Texture = Surface.ColorPassTexture,
                .NextUsage = kraft::renderer::TextureLayout::RenderTarget,
                .ClearColor = { 0.10f, 0.10f, 0.10f, 1.0f },
            }
        },
        .DepthTarget = {
            .Texture = Surface.DepthPassTexture,
            .NextUsage = kraft::renderer::TextureLayout::DepthStencil,
            .Depth = 1.0f,
            .Stencil = 0,
        },
        .Layout = {
            .Subpasses = {
                {
                    .DepthTarget = true,
                    .ColorTargetSlots = { 0 }
                }
            }
        }
    });

	Surface.GlobalUBO = kraft::renderer::ResourceManager::Ptr->CreateBuffer({
		.DebugName = "ObjectPickingGlobalUBO",
		.Size = sizeof(kraft::renderer::GlobalShaderData),
		.UsageFlags = kraft::renderer::BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | kraft::renderer::BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
		.MemoryPropertyFlags = kraft::renderer::MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | kraft::renderer::MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | kraft::renderer::MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
		.MapMemory = true,
	});

    for (int i = 0; i < 3; i++)
    {
        Surface.CmdBuffers[i] = kraft::renderer::ResourceManager::Ptr->CreateCommandBuffer({
            .DebugName = "ObjectPickingCmdBuf",
            .Primary = true,
        });
    }

    this->ObjectPickingRenderTarget = Surface;

    this->ObjectPickingShader = kraft::ShaderSystem::AcquireShader("res/shaders/object_picking.kfx.bkfx", Surface.RenderPass);
}

kraft::Entity EditorState::GetSelectedEntity()
{
    return this->CurrentWorld->GetEntity(SelectedEntity);
}

void EditorState::SetCamera(const kraft::Camera& Camera)
{
    this->CurrentWorld->Camera = Camera;
}
