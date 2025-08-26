#include "kraft_renderer_frontend.h"

#include <containers/kraft_hashmap.h>
#include <core/kraft_allocators.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_time.h>
#include <renderer/kraft_camera.h>
#include <renderer/kraft_resource_manager.h>
#include <renderer/vulkan/kraft_vulkan_backend.h>
#include <renderer/vulkan/kraft_vulkan_resource_manager.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>
#include <world/kraft_entity.h>
#include <world/kraft_world.h>

#include <kraft_types.h>
#include <resources/kraft_resource_types.h>
#include <shaderfx/kraft_shaderfx_types.h>

#include <math/kraft_math.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>

namespace kraft {
renderer::RendererFrontend* g_Renderer = nullptr;
renderer::GPUDevice*        g_Device = nullptr;
} // namespace kraft

namespace kraft::renderer {

using Renderables = FlatHashMap<ResourceID, Array<Renderable>>;

struct RenderDataT
{
    RenderSurfaceT Surface;
    Renderables    Renderables;
};

struct ResourceManager* ResourceManager = nullptr;

struct RendererFrontendPrivate
{
    Renderables        renderables;
    Array<RenderDataT> surfaces;
    RendererBackend*   backend = nullptr;
    int                active_surface = -1;
    int                current_frame_index = -1;
    ArenaAllocator*    arena;

    Handle<Buffer> global_ubo_buffer;
    Handle<Buffer> materials_gpu_buffer;
    Handle<Buffer> materials_staging_buffer[3];
} renderer_data_internal;

void RendererFrontend::Init()
{
    uint64 MaterialsBufferSize = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;
    renderer_data_internal.materials_gpu_buffer = ResourceManager->CreateBuffer({
        .Size = MaterialsBufferSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER | BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .SharingMode = SharingMode::Exclusive,
    });

    for (int i = 0; i < KRAFT_C_ARRAY_SIZE(renderer_data_internal.materials_staging_buffer); i++)
    {
        renderer_data_internal.materials_staging_buffer[i] = ResourceManager->CreateBuffer({
            .Size = MaterialsBufferSize,
            .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_SRC,
            .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
            .SharingMode = SharingMode::Exclusive,
            .MapMemory = true,
        });
    }

    renderer_data_internal.global_ubo_buffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalUBO",
        .Size = sizeof(GlobalShaderData),
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
        .MemoryPropertyFlags =
            MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .MapMemory = true,
    });
}

struct __DrawData
{
    Mat4f  Model;
    Vec2f  MousePosition;
    uint32 EntityId;
    uint32 MaterialIdx;
} DummyDrawData;

void RendererFrontend::Draw(Shader* Shader, GlobalShaderData* GlobalUBO, uint32 GeometryId)
{
    KASSERT(renderer_data_internal.current_frame_index >= 0 && renderer_data_internal.current_frame_index < 3);

    MemCpy((void*)ResourceManager->GetBufferData(renderer_data_internal.global_ubo_buffer), (void*)GlobalUBO, sizeof(GlobalShaderData));

    renderer_data_internal.backend->UseShader(Shader);
    renderer_data_internal.backend->ApplyGlobalShaderProperties(Shader, renderer_data_internal.global_ubo_buffer, renderer_data_internal.materials_gpu_buffer);

    DummyDrawData.Model = kraft::ScaleMatrix(kraft::Vec3f{ 1920.0f * 1.2f, 945.0f * 1.2f, 1.0f });
    DummyDrawData.MaterialIdx = 0;
    renderer_data_internal.backend->ApplyLocalShaderProperties(Shader, &DummyDrawData);

    renderer_data_internal.backend->DrawGeometryData(GeometryId);
}

void RendererFrontend::OnResize(int width, int height)
{
    KASSERT(renderer_data_internal.backend);
    renderer_data_internal.backend->OnResize(width, height);
}

void RendererFrontend::PrepareFrame()
{
    renderer::ResourceManager->EndFrame(0);
    renderer_data_internal.current_frame_index = renderer_data_internal.backend->PrepareFrame();

    // Upload all the materials
    uint8* BufferData = ResourceManager->GetBufferData(renderer_data_internal.materials_staging_buffer[renderer_data_internal.current_frame_index]);
    auto   Materials = MaterialSystem::GetMaterialsBuffer();
    uint64 MaterialsBufferSize = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;

    MemCpy(BufferData, Materials, MaterialsBufferSize);

    ResourceManager->UploadBuffer({
        .DstBuffer = renderer_data_internal.materials_gpu_buffer,
        .SrcBuffer = renderer_data_internal.materials_staging_buffer[renderer_data_internal.current_frame_index],
        .SrcSize = MaterialsBufferSize,
        .DstOffset = 0,
        .SrcOffset = 0,
    });

    // Get all the dirty textures and create descriptor sets for them
    auto DirtyTextures = TextureSystem::GetDirtyTextures();
    if (DirtyTextures.Length > 0)
    {
        renderer_data_internal.backend->UpdateTextures(DirtyTextures.Data(), DirtyTextures.Length);
        TextureSystem::ClearDirtyTextures();
    }
}

bool RendererFrontend::DrawSurfaces()
{
    KASSERTM(renderer_data_internal.current_frame_index >= 0, "Did you forget to call Renderer.PrepareFrame()?");

    GlobalShaderData GlobalShaderData = {};
    GlobalShaderData.Projection = this->Camera->ProjectionMatrix;
    GlobalShaderData.View = this->Camera->GetViewMatrix();
    GlobalShaderData.CameraPosition = this->Camera->Position;

    uint64 Start = kraft::Platform::TimeNowNS();
    for (int i = 0; i < renderer_data_internal.surfaces.Length; i++)
    {
        RenderDataT&    SurfaceRenderData = renderer_data_internal.surfaces[i];
        RenderSurfaceT& Surface = SurfaceRenderData.Surface;

        if (Surface.World->GlobalLight != EntityHandleInvalid)
        {
            Entity GlobalLight = Surface.World->GetEntity(Surface.World->GlobalLight);
            GlobalShaderData.GlobalLightColor = GlobalLight.GetComponent<LightComponent>().LightColor;
            GlobalShaderData.GlobalLightPosition = GlobalLight.GetComponent<TransformComponent>().Position;
        }

        MemCpy((void*)ResourceManager->GetBufferData(Surface.GlobalUBO), (void*)&GlobalShaderData, sizeof(GlobalShaderData));

        Surface.Begin();
        for (auto It = SurfaceRenderData.Renderables.begin(); It != SurfaceRenderData.Renderables.end(); It++)
        {
            Shader* CurrentShader = ShaderSystem::BindByID(It->first);
            renderer_data_internal.backend->ApplyGlobalShaderProperties(CurrentShader, Surface.GlobalUBO, renderer_data_internal.materials_gpu_buffer);

            auto&  Objects = It->second;
            uint64 Count = Objects.Size();
            if (!Count)
                continue;

            for (int i = 0; i < Count; i++)
            {
                Renderable Object = Objects[i];

                DummyDrawData.Model = Object.ModelMatrix;
                DummyDrawData.MaterialIdx = Object.MaterialInstance->ID;
                DummyDrawData.MousePosition = Surface.RelativeMousePosition;
                DummyDrawData.EntityId = Object.EntityId;
                renderer_data_internal.backend->ApplyLocalShaderProperties(CurrentShader, &DummyDrawData);

                renderer_data_internal.backend->DrawGeometryData(Object.GeometryId);
            }

            Objects.Clear();

            ShaderSystem::Unbind();
        }
        Surface.End();
    }
    uint64 End = kraft::Platform::TimeNowNS();

    // KDEBUG("Render complete in: %f ms", f64(End - Start) / 1000000.0f);

    renderer_data_internal.surfaces.Clear();

    return true;
}

bool RendererFrontend::AddRenderable(const Renderable& renderable)
{
    Renderables* Renderables = nullptr;
    if (renderer_data_internal.active_surface != -1)
    {
        Renderables = &renderer_data_internal.surfaces[renderer_data_internal.active_surface].Renderables;
    }
    else
    {
        Renderables = &renderer_data_internal.renderables;
    }

    ResourceID Key = renderable.MaterialInstance->Shader->ID;
    auto       RenderablesIt = Renderables->find(Key);
    if (RenderablesIt == Renderables->end())
    {
        auto Pair = Renderables->insert_or_assign(Key, Array<Renderable>());
        auto It = Pair.first;
        It->second.Reserve(1024);
        It->second.Push(renderable);
    }
    else
    {
        RenderablesIt->second.Push(renderable);
    }

    return true;
}

void RendererFrontend::BeginMainRenderpass()
{
    renderer_data_internal.backend->BeginFrame();
}

void RendererFrontend::EndMainRenderpass()
{
    renderer_data_internal.backend->EndFrame();
}

//
// API
//

void RendererFrontend::CreateRenderPipeline(Shader* Shader, int PassIndex, Handle<RenderPass> RenderPassHandle)
{
    renderer_data_internal.backend->CreateRenderPipeline(Shader, PassIndex, RenderPassHandle);
}

void RendererFrontend::DestroyRenderPipeline(Shader* Shader)
{
    renderer_data_internal.backend->DestroyRenderPipeline(Shader);
}

void RendererFrontend::UseShader(const Shader* Shader)
{
    renderer_data_internal.backend->UseShader(Shader);
}

void RendererFrontend::DrawGeometry(uint32 GeometryID)
{
    renderer_data_internal.backend->DrawGeometryData(GeometryID);
}

void RendererFrontend::ApplyGlobalShaderProperties(Shader* ActiveShader, Handle<Buffer> GlobalUBOBuffer, Handle<Buffer> GlobalMaterialsBuffer)
{
    renderer_data_internal.backend->ApplyGlobalShaderProperties(ActiveShader, GlobalUBOBuffer, GlobalMaterialsBuffer);
}

void RendererFrontend::ApplyLocalShaderProperties(Shader* ActiveShader, void* Data)
{
    renderer_data_internal.backend->ApplyLocalShaderProperties(ActiveShader, Data);
}

bool RendererFrontend::CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize)
{
    return renderer_data_internal.backend->CreateGeometry(Geometry, VertexCount, Vertices, VertexSize, IndexCount, Indices, IndexSize);
}

void RendererFrontend::DestroyGeometry(Geometry* Geometry)
{
    renderer_data_internal.backend->DestroyGeometry(Geometry);
}

RenderSurfaceT RendererFrontend::CreateRenderSurface(const char* Name, uint32 Width, uint32 Height, bool HasDepth)
{
    RenderSurfaceT Surface = {
        .DebugName = Name,
        .Width = Width,
        .Height = Height,
    };

    Surface.ColorPassTexture = ResourceManager->CreateTexture({
        .DebugName = "ColorPassTexture",
        .Dimensions = { (float32)Width, (float32)Height, 1, 4 },
        .Format = Format::BGRA8_UNORM,
        .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT | TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
    });

    Surface.TextureSampler = ResourceManager->CreateTextureSampler({
        .WrapModeU = TextureWrapMode::ClampToEdge,
        .WrapModeV = TextureWrapMode::ClampToEdge,
        .WrapModeW = TextureWrapMode::ClampToEdge,
        .Compare = CompareOp::Always,
    });

    if (HasDepth)
    {
        Surface.DepthPassTexture = ResourceManager->CreateTexture({
            .DebugName = "DepthPassTexture",
            .Dimensions = { (float32)Width, (float32)Height, 1, 4 },
            .Format = ResourceManager->GetPhysicalDeviceFormatSpecs().DepthBufferFormat,
            .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT,
        });
    }

    Surface.RenderPass = ResourceManager->CreateRenderPass({
        .DebugName = Name,
        .Dimensions = { (float)Width, (float)Height, 0.f, 0.f },
        .ColorTargets = {
            {
                .Texture = Surface.ColorPassTexture,
                .NextUsage = TextureLayout::Sampled,
                .ClearColor = { 0.10f, 0.10f, 0.10f, 1.0f },
            }
        },
        .DepthTarget = {
            .Texture = Surface.DepthPassTexture,
            .NextUsage = TextureLayout::DepthStencil,
            .Depth = 1.0f,
            .Stencil = 0,
        },
        .Layout = {
            .Subpasses = {
                {
                    .DepthTarget = HasDepth,
                    .ColorTargetSlots = { 0 }
                }
            }
        }
    });

    Surface.GlobalUBO = ResourceManager->CreateBuffer({
        .DebugName = Name,
        .Size = sizeof(GlobalShaderData),
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
        .MemoryPropertyFlags =
            MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .MapMemory = true,
    });

    for (int i = 0; i < 3; i++)
    {
        Surface.CmdBuffers[i] = ResourceManager->CreateCommandBuffer({
            .DebugName = Name,
            .Primary = true,
        });
    }

    return Surface;
}

void RendererFrontend::BeginRenderSurface(const RenderSurfaceT& Surface)
{
    renderer_data_internal.surfaces.Push({
        .Surface = Surface,
    });

    renderer_data_internal.active_surface = renderer_data_internal.surfaces.Length - 1;
    // RendererData.Backend->BeginRenderPass(Surface.CmdBuffer, Surface.RenderPass);
}

RenderSurfaceT RendererFrontend::ResizeRenderSurface(RenderSurfaceT& Surface, uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0)
    {
        return Surface;
    }

    //Width *= 0.5f;
    //Height *= 0.5f;

    KDEBUG("Viewport size set to = %d, %d", Width, Height);

    // Delete the old color pass and depth pass
    ResourceManager->DestroyRenderPass(Surface.RenderPass);
    ResourceManager->DestroyTexture(Surface.ColorPassTexture);
    if (Surface.DepthPassTexture)
    {
        ResourceManager->DestroyTexture(Surface.DepthPassTexture);
    }

    // this->CreateCommandBuffers();
    Surface = this->CreateRenderSurface(Surface.DebugName, Width, Height, Surface.DepthPassTexture != Handle<Texture>::Invalid());
    return Surface;
}

void RendererFrontend::EndRenderSurface(const RenderSurfaceT& Surface)
{
    renderer_data_internal.active_surface = -1;
    // RendererData.Backend->EndRenderPass(Surface.CmdBuffer, Surface.RenderPass);
}

void RendererFrontend::CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, uint32 set_idx, uint32 binding_idx)
{
    renderer_data_internal.backend->CmdSetCustomBuffer(shader, buffer, set_idx, binding_idx);
}

void RenderSurfaceT::Begin()
{
    renderer_data_internal.backend->BeginRenderPass(this->CmdBuffers[renderer_data_internal.current_frame_index], this->RenderPass);
}

void RenderSurfaceT::End()
{
    renderer_data_internal.backend->EndRenderPass(this->CmdBuffers[renderer_data_internal.current_frame_index], this->RenderPass);
}

RendererFrontend* CreateRendererFrontend(const RendererOptions* Opts)
{
    KASSERT(g_Renderer == nullptr);

    renderer_data_internal.arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(1),
        .Alignment = 64,
    });

    g_Renderer = ArenaPush(renderer_data_internal.arena, RendererFrontend);
    renderer_data_internal.backend = ArenaPush(renderer_data_internal.arena, RendererBackend);
    g_Device = ArenaPush(renderer_data_internal.arena, GPUDevice);

    g_Renderer->Settings = ArenaPush(renderer_data_internal.arena, RendererOptions);
    MemCpy(g_Renderer->Settings, Opts, sizeof(RendererOptions));
    KASSERTM(g_Renderer->Settings->Backend != RendererBackendType::RENDERER_BACKEND_TYPE_NONE, "No renderer backend specified");

    ResourceManager = CreateVulkanResourceManager(renderer_data_internal.arena);

    if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        renderer_data_internal.backend->Init = VulkanRendererBackend::Init;
        renderer_data_internal.backend->Shutdown = VulkanRendererBackend::Shutdown;
        renderer_data_internal.backend->PrepareFrame = VulkanRendererBackend::PrepareFrame;
        renderer_data_internal.backend->BeginFrame = VulkanRendererBackend::BeginFrame;
        renderer_data_internal.backend->EndFrame = VulkanRendererBackend::EndFrame;
        renderer_data_internal.backend->OnResize = VulkanRendererBackend::OnResize;
        renderer_data_internal.backend->CreateRenderPipeline = VulkanRendererBackend::CreateRenderPipeline;
        renderer_data_internal.backend->DestroyRenderPipeline = VulkanRendererBackend::DestroyRenderPipeline;
        renderer_data_internal.backend->UseShader = VulkanRendererBackend::UseShader;
        renderer_data_internal.backend->ApplyGlobalShaderProperties = VulkanRendererBackend::ApplyGlobalShaderProperties;
        renderer_data_internal.backend->ApplyLocalShaderProperties = VulkanRendererBackend::ApplyLocalShaderProperties;
        renderer_data_internal.backend->UpdateTextures = VulkanRendererBackend::UpdateTextures;
        renderer_data_internal.backend->CreateGeometry = VulkanRendererBackend::CreateGeometry;
        renderer_data_internal.backend->DrawGeometryData = VulkanRendererBackend::DrawGeometryData;
        renderer_data_internal.backend->DestroyGeometry = VulkanRendererBackend::DestroyGeometry;

        // Render Passes
        renderer_data_internal.backend->BeginRenderPass = VulkanRendererBackend::BeginRenderPass;
        renderer_data_internal.backend->EndRenderPass = VulkanRendererBackend::EndRenderPass;

        // Commands
        renderer_data_internal.backend->CmdSetCustomBuffer = VulkanRendererBackend::CmdSetCustomBuffer;
    }
    else if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {
        return nullptr;
    }

    if (!renderer_data_internal.backend->Init(renderer_data_internal.arena, g_Renderer->Settings))
    {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return nullptr;
    }

    // Post-init
    if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        VulkanRendererBackend::SetDeviceData(g_Device);
    }

    g_Renderer->Init();

    return g_Renderer;
}

void DestroyRendererFrontend(RendererFrontend* Instance)
{
    renderer_data_internal.backend->Shutdown();
    MemZero(renderer_data_internal.backend, sizeof(RendererBackend));

    DestroyVulkanResourceManager(ResourceManager);
    DestroyArena(renderer_data_internal.arena);
}

} // namespace kraft::renderer