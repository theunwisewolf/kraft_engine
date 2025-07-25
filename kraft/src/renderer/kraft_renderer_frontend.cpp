#include "kraft_renderer_frontend.h"

#include <containers/kraft_hashmap.h>
#include <core/kraft_allocators.h>
#include <core/kraft_asserts.h>
#include <core/kraft_engine.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
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
}

namespace kraft::renderer {

using RenderablesT = FlatHashMap<ResourceID, FlatHashMap<ResourceID, Array<Renderable>>>;

struct RenderDataT
{
    RenderSurfaceT Surface;
    RenderablesT   Renderables;
};

struct ResourceManager* ResourceManager = nullptr;

struct RendererDataT
{
    MemoryBlock        BackendMemory;
    RenderablesT       Renderables;
    Array<RenderDataT> RenderSurfaces;
    RendererBackend*   Backend = nullptr;
    int                ActiveSurface = -1;
    int                CurrentFrameIndex = -1;
    ArenaAllocator*    Arena;

    Handle<Buffer> GlobalUBO;
    Handle<Buffer> MaterialsGPUBuffer;
    Handle<Buffer> MaterialsStagingBuffers[3];
} RendererData;

void RendererFrontend::Init()
{
    uint64 MaterialsBufferSize = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;
    RendererData.MaterialsGPUBuffer = ResourceManager->CreateBuffer({
        .Size = MaterialsBufferSize,
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER | BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .SharingMode = SharingMode::Exclusive,
    });

    for (int i = 0; i < KRAFT_C_ARRAY_SIZE(RendererData.MaterialsStagingBuffers); i++)
    {
        RendererData.MaterialsStagingBuffers[i] = ResourceManager->CreateBuffer({
            .Size = MaterialsBufferSize,
            .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_SRC,
            .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
            .SharingMode = SharingMode::Exclusive,
            .MapMemory = true,
        });
    }

    RendererData.GlobalUBO = ResourceManager->CreateBuffer({
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
    KASSERT(RendererData.CurrentFrameIndex >= 0 && RendererData.CurrentFrameIndex < 3);

    uint8* BufferData = ResourceManager->GetBufferData(RendererData.MaterialsStagingBuffers[RendererData.CurrentFrameIndex]);
    auto   Materials = MaterialSystem::GetMaterialsBuffer();
    uint64 MaterialsBufferSize = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;

    MemCpy(BufferData, Materials, MaterialsBufferSize);

    ResourceManager->UploadBuffer({
        .DstBuffer = RendererData.MaterialsGPUBuffer,
        .SrcBuffer = RendererData.MaterialsStagingBuffers[RendererData.CurrentFrameIndex],
        .SrcSize = MaterialsBufferSize,
        .DstOffset = 0,
        .SrcOffset = 0,
    });

    MemCpy((void*)ResourceManager->GetBufferData(RendererData.GlobalUBO), (void*)GlobalUBO, sizeof(GlobalShaderData));

    RendererData.Backend->UseShader(Shader);
    RendererData.Backend->ApplyGlobalShaderProperties(Shader, RendererData.GlobalUBO, RendererData.MaterialsGPUBuffer);

    DummyDrawData.Model = kraft::ScaleMatrix(kraft::Vec3f{ 300.0f, 300.0f, 300.0f });
    DummyDrawData.MaterialIdx = 0;
    RendererData.Backend->ApplyLocalShaderProperties(Shader, &DummyDrawData);

    RendererData.Backend->DrawGeometryData(GeometryId);
}

void RendererFrontend::OnResize(int width, int height)
{
    if (RendererData.Backend)
    {
        RendererData.Backend->OnResize(width, height);
    }
    else
    {
        KERROR("[RendererFrontend::OnResize]: No backend!");
    }
}

void RendererFrontend::PrepareFrame()
{
    renderer::ResourceManager->EndFrame(0);
    RendererData.CurrentFrameIndex = RendererData.Backend->PrepareFrame();

    // Upload all the materials
    uint8* BufferData = ResourceManager->GetBufferData(RendererData.MaterialsStagingBuffers[RendererData.CurrentFrameIndex]);
    auto   Materials = MaterialSystem::GetMaterialsBuffer();
    uint64 MaterialsBufferSize = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;

    MemCpy(BufferData, Materials, MaterialsBufferSize);

    ResourceManager->UploadBuffer({
        .DstBuffer = RendererData.MaterialsGPUBuffer,
        .SrcBuffer = RendererData.MaterialsStagingBuffers[RendererData.CurrentFrameIndex],
        .SrcSize = MaterialsBufferSize,
        .DstOffset = 0,
        .SrcOffset = 0,
    });

    // Get all the dirty textures and create descriptor sets for them
    auto DirtyTextures = TextureSystem::GetDirtyTextures();
    if (DirtyTextures.Length > 0)
    {
        RendererData.Backend->UpdateTextures(DirtyTextures);
        TextureSystem::ClearDirtyTextures();
    }
}

bool RendererFrontend::DrawSurfaces()
{
    KASSERTM(RendererData.CurrentFrameIndex >= 0, "Did you forget to call Renderer.PrepareFrame()?");

    GlobalShaderData GlobalShaderData = {};
    GlobalShaderData.Projection = this->Camera->ProjectionMatrix;
    GlobalShaderData.View = this->Camera->GetViewMatrix();
    GlobalShaderData.CameraPosition = this->Camera->Position;

    for (int i = 0; i < RendererData.RenderSurfaces.Length; i++)
    {
        RenderDataT&    SurfaceRenderData = RendererData.RenderSurfaces[i];
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
            RendererData.Backend->ApplyGlobalShaderProperties(CurrentShader, Surface.GlobalUBO, RendererData.MaterialsGPUBuffer);
            for (auto& MaterialIt : It->second)
            {
                auto&  Objects = MaterialIt.second;
                uint64 Count = Objects.Size();
                if (!Count)
                    continue;
                // ShaderSystem::Bind(Objects[0].MaterialInstance->Shader);
                ShaderSystem::SetMaterialInstance(Objects[0].MaterialInstance);
                for (int i = 0; i < Count; i++)
                {
                    Renderable Object = Objects[i];

                    // MaterialSystem::ApplyInstanceProperties(Object.MaterialInstance);
                    // MaterialSystem::ApplyLocalProperties(Object.MaterialInstance, Object.ModelMatrix);

                    // float64 x, y;
                    // kraft::Platform::GetWindow()->GetCursorPosition(&x, &y);
                    // Vec2f MousePosition{ (float32)x, (float32)y };
                    // ShaderSystem::SetUniform("MousePosition", kraft::Vec2f{ Surface.RelativeMousePosition.x, Surface.RelativeMousePosition.y });

                    // uint8 Buffer[128] = { 0 };
                    // MemCpy(Buffer, Object.ModelMatrix._data, sizeof(Object.ModelMatrix));
                    // MemCpy(Buffer + sizeof(Object.ModelMatrix), Surface.RelativeMousePosition._data, sizeof(Surface.RelativeMousePosition));
                    // MemCpy(Buffer + sizeof(Object.ModelMatrix) + sizeof(Surface.RelativeMousePosition), &Object.EntityId, sizeof(Object.EntityId));

                    // RendererData.Backend->SetUniform(CurrentShader, kraft::ShaderUniform{ .Offset = 0, .Stride = 128, .Scope = ShaderUniformScope::Local }, Buffer, true);
                    // ShaderSystem::SetUniform("EntityId", Object.EntityId);

                    DummyDrawData.Model = Object.ModelMatrix;
                    DummyDrawData.MaterialIdx = Object.MaterialInstance->ID;
                    DummyDrawData.MousePosition = Surface.RelativeMousePosition;
                    DummyDrawData.EntityId = Object.EntityId;
                    RendererData.Backend->ApplyLocalShaderProperties(CurrentShader, &DummyDrawData);

                    // uint8* MaterialsBuffer = MaterialSystem::GetMaterialsBuffer();
                    // uint8* MaterialBuffer = MaterialsBuffer + DummyDrawData.MaterialIdx * Settings->MaterialBufferSize;
                    // Vec4f* Ambient = (Vec4f*)(MaterialBuffer);
                    // Vec4f* Diffuse = (Vec4f*)((uint8*)Ambient + sizeof(Vec4f));
                    // Vec4f* Specular = (Vec4f*)((uint8*)Diffuse + sizeof(Vec4f));
                    // float* Shininess = (float*)((uint8*)Specular + sizeof(Vec4f));
                    // // uint32* TextureHandle = (uint32*)((uint8*)Shininess + sizeof(Vec4f));
                    // uint32* TextureHandle = (uint32*)(MaterialBuffer + 52);

                    // MetadataComponent Metadata = Surface.World->GetEntity(Object.EntityId).GetComponent<MetadataComponent>();

                    // KDEBUG("-------------------------------------");
                    // KDEBUG("%s", *Metadata.Name);
                    // KDEBUG("Ambient = (%f, %f, %f, %f)", Ambient->x, Ambient->y, Ambient->z, Ambient->w);
                    // KDEBUG("Diffuse = (%f, %f, %f, %f)", Diffuse->x, Diffuse->y, Diffuse->z, Diffuse->w);
                    // KDEBUG("Specular = (%f, %f, %f, %f)", Specular->x, Specular->y, Specular->z, Specular->w);
                    // KDEBUG("Shininess = %f", *Shininess);
                    // KDEBUG("TextureHandle = %d", *TextureHandle);
                    // KDEBUG("-------------------------------------");

                    RendererData.Backend->DrawGeometryData(Object.GeometryId);
                }

                Objects.Clear();
            }

            ShaderSystem::Unbind();
        }
        Surface.End();
    }

    RendererData.RenderSurfaces.Clear();

    return true;
}

bool RendererFrontend::AddRenderable(const Renderable& Object)
{
    RenderablesT* Renderables = nullptr;
    if (RendererData.ActiveSurface != -1)
    {
        RenderSurfaceT& Surface = RendererData.RenderSurfaces[RendererData.ActiveSurface].Surface;
        Renderables = &RendererData.RenderSurfaces[RendererData.ActiveSurface].Renderables;
    }
    else
    {
        Renderables = &RendererData.Renderables;
    }

    ResourceID Key = Object.MaterialInstance->Shader->ID;
    // ResourceID Key = Object.MaterialInstance->ID;
    auto RenderablesIt = Renderables->find(Key);
    if (RenderablesIt == Renderables->end())
    {
        auto Pair = Renderables->insert_or_assign(Key, FlatHashMap<ResourceID, Array<Renderable>>());
        auto It = Pair.first;
        auto ArrayPair = It->second.insert_or_assign(Object.MaterialInstance->ID, Array<Renderable>());
        auto ArrayIt = ArrayPair.first;
        ArrayIt->second.Reserve(1024);
        ArrayIt->second.Push(Object);
    }
    else
    {
        RenderablesIt->second[Object.MaterialInstance->ID].Push(Object);
    }

    // (*Renderables)[Key].Push(Object);

    return true;
}

void RendererFrontend::BeginMainRenderpass()
{
    RendererData.Backend->BeginFrame();
}

void RendererFrontend::EndMainRenderpass()
{
    RendererData.Backend->EndFrame();
}

//
// API
//

void RendererFrontend::CreateRenderPipeline(Shader* Shader, int PassIndex, Handle<RenderPass> RenderPassHandle)
{
    RendererData.Backend->CreateRenderPipeline(Shader, PassIndex, RenderPassHandle);
}

void RendererFrontend::DestroyRenderPipeline(Shader* Shader)
{
    RendererData.Backend->DestroyRenderPipeline(Shader);
}

void RendererFrontend::CreateMaterial(Material* Material)
{
    RendererData.Backend->CreateMaterial(Material);
}

void RendererFrontend::DestroyMaterial(Material* Instance)
{
    RendererData.Backend->DestroyMaterial(Instance);
}

void RendererFrontend::UseShader(const Shader* Shader)
{
    RendererData.Backend->UseShader(Shader);
}

void RendererFrontend::SetUniform(Shader* ActiveShader, const ShaderUniform& Uniform, void* Value, bool Invalidate)
{
    RendererData.Backend->SetUniform(ActiveShader, Uniform, Value, Invalidate);
}

void RendererFrontend::DrawGeometry(uint32 GeometryID)
{
    RendererData.Backend->DrawGeometryData(GeometryID);
}

void RendererFrontend::ApplyGlobalShaderProperties(Shader* ActiveShader, Handle<Buffer> GlobalUBOBuffer, Handle<Buffer> GlobalMaterialsBuffer)
{
    RendererData.Backend->ApplyGlobalShaderProperties(ActiveShader, GlobalUBOBuffer, GlobalMaterialsBuffer);
}

void RendererFrontend::ApplyInstanceShaderProperties(Shader* ActiveShader)
{
    RendererData.Backend->ApplyInstanceShaderProperties(ActiveShader);
}

bool RendererFrontend::CreateGeometry(Geometry* Geometry, uint32 VertexCount, const void* Vertices, uint32 VertexSize, uint32 IndexCount, const void* Indices, const uint32 IndexSize)
{
    return RendererData.Backend->CreateGeometry(Geometry, VertexCount, Vertices, VertexSize, IndexCount, Indices, IndexSize);
}

void RendererFrontend::DestroyGeometry(Geometry* Geometry)
{
    RendererData.Backend->DestroyGeometry(Geometry);
}

bool RendererFrontend::ReadObjectPickingBuffer(uint32** OutBuffer, uint32* BufferSize)
{
    return RendererData.Backend->ReadObjectPickingBuffer(OutBuffer, BufferSize);
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
    RendererData.ActiveSurface = RendererData.RenderSurfaces.Push({
        .Surface = Surface,
    });
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
    RendererData.ActiveSurface = -1;
    // RendererData.Backend->EndRenderPass(Surface.CmdBuffer, Surface.RenderPass);
}

void RenderSurfaceT::Begin()
{
    RendererData.Backend->BeginRenderPass(this->CmdBuffers[RendererData.CurrentFrameIndex], this->RenderPass);
}

void RenderSurfaceT::End()
{
    RendererData.Backend->EndRenderPass(this->CmdBuffers[RendererData.CurrentFrameIndex], this->RenderPass);
}

RendererFrontend* CreateRendererFrontend(const RendererOptions* Opts)
{
    KASSERT(g_Renderer == nullptr);

    RendererData.Arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(1),
        .Alignment = 64,
    });

    g_Renderer = (RendererFrontend*)RendererData.Arena->Push(sizeof(RendererFrontend), true);
    RendererData.Backend = (RendererBackend*)RendererData.Arena->Push(sizeof(RendererBackend), true);

    g_Renderer->Settings = (struct RendererOptions*)RendererData.Arena->Push(sizeof(RendererOptions), true);
    MemCpy(g_Renderer->Settings, Opts, sizeof(RendererOptions));
    KASSERTM(g_Renderer->Settings->Backend != RendererBackendType::RENDERER_BACKEND_TYPE_NONE, "No renderer backend specified");

    ResourceManager = CreateVulkanResourceManager(RendererData.Arena);

    if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        RendererData.Backend->Init = VulkanRendererBackend::Init;
        RendererData.Backend->Shutdown = VulkanRendererBackend::Shutdown;
        RendererData.Backend->PrepareFrame = VulkanRendererBackend::PrepareFrame;
        RendererData.Backend->BeginFrame = VulkanRendererBackend::BeginFrame;
        RendererData.Backend->EndFrame = VulkanRendererBackend::EndFrame;
        RendererData.Backend->OnResize = VulkanRendererBackend::OnResize;
        RendererData.Backend->CreateRenderPipeline = VulkanRendererBackend::CreateRenderPipeline;
        RendererData.Backend->DestroyRenderPipeline = VulkanRendererBackend::DestroyRenderPipeline;
        RendererData.Backend->UseShader = VulkanRendererBackend::UseShader;
        RendererData.Backend->SetUniform = VulkanRendererBackend::SetUniform;
        RendererData.Backend->ApplyGlobalShaderProperties = VulkanRendererBackend::ApplyGlobalShaderProperties;
        RendererData.Backend->ApplyInstanceShaderProperties = VulkanRendererBackend::ApplyInstanceShaderProperties;
        RendererData.Backend->ApplyLocalShaderProperties = VulkanRendererBackend::ApplyLocalShaderProperties;
        RendererData.Backend->UpdateTextures = VulkanRendererBackend::UpdateTextures;
        RendererData.Backend->CreateMaterial = VulkanRendererBackend::CreateMaterial;
        RendererData.Backend->DestroyMaterial = VulkanRendererBackend::DestroyMaterial;
        RendererData.Backend->CreateGeometry = VulkanRendererBackend::CreateGeometry;
        RendererData.Backend->DrawGeometryData = VulkanRendererBackend::DrawGeometryData;
        RendererData.Backend->DestroyGeometry = VulkanRendererBackend::DestroyGeometry;

        //
        RendererData.Backend->ReadObjectPickingBuffer = VulkanRendererBackend::ReadObjectPickingBuffer;

        // Render Passes
        RendererData.Backend->BeginRenderPass = VulkanRendererBackend::BeginRenderPass;
        RendererData.Backend->EndRenderPass = VulkanRendererBackend::EndRenderPass;
    }
    else if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {
        return nullptr;
    }

    if (!RendererData.Backend->Init(RendererData.Arena, g_Renderer->Settings))
    {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return nullptr;
    }

    g_Renderer->Init();

    return g_Renderer;
}

void DestroyRendererFrontend(RendererFrontend* Instance)
{
    bool RetVal = RendererData.Backend->Shutdown();
    MemZero(RendererData.Backend, sizeof(RendererBackend));

    DestroyVulkanResourceManager(ResourceManager);
    DestroyArena(RendererData.Arena);
}

} // namespace kraft::renderer