#include "kraft_renderer_frontend.h"

#include <containers/kraft_hashmap.h>
#include <core/kraft_asserts.h>
#include <core/kraft_engine.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <renderer/kraft_camera.h>
#include <renderer/kraft_renderer_backend.h>
#include <renderer/kraft_resource_manager.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <world/kraft_entity.h>
#include <world/kraft_world.h>

#include <renderer/shaderfx/kraft_shaderfx_types.h>
#include <resources/kraft_resource_types.h>

namespace kraft::renderer {

using RenderablesT = FlatHashMap<ResourceID, FlatHashMap<ResourceID, Array<Renderable>>>;

struct RenderDataT
{
    RenderSurfaceT Surface;
    RenderablesT   Renderables;
};

struct RendererDataT
{
    MemoryBlock         BackendMemory;
    RendererBackendType Type = RendererBackendType::RENDERER_BACKEND_TYPE_NONE;
    RenderablesT        Renderables;
    Array<RenderDataT>  RenderSurfaces;
    RendererBackend*    Backend = nullptr;
    int                 ActiveSurface = -1;
    int                 CurrentFrameIndex = -1;
} RendererData;

RendererFrontend* Renderer = nullptr;

bool RendererFrontend::Init(EngineConfigT* Config)
{
    Renderer = this;
    RendererData.Type = Config->RendererBackend;
    RendererData.BackendMemory = MallocBlock(sizeof(RendererBackend), MEMORY_TAG_RENDERER);
    RendererData.Backend = (RendererBackend*)RendererData.BackendMemory.Data;
    RendererData.Renderables.reserve(1024);

    KASSERTM(RendererData.Type != RendererBackendType::RENDERER_BACKEND_TYPE_NONE, "No renderer backend specified");

    if (!CreateBackend(RendererData.Type, RendererData.Backend))
    {
        return false;
    }

    if (!RendererData.Backend->Init(Config))
    {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return false;
    }

    return true;
}

bool RendererFrontend::Shutdown()
{
    DestroyBackend(RendererData.Backend);

    FreeBlock(RendererData.BackendMemory);
    RendererData.Backend = nullptr;

    return true;
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
    renderer::ResourceManager::Ptr->EndFrame(0);
    RendererData.CurrentFrameIndex = RendererData.Backend->PrepareFrame();
}

bool RendererFrontend::DrawSurfaces()
{
    KASSERTM(RendererData.CurrentFrameIndex >= 0, "Did you forget to call Renderer.PrepareFrame()?");

    GlobalUniformData GlobalShaderData = {};
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

        Surface.Begin();
        ShaderSystem::ApplyGlobalProperties(GlobalShaderData);
        for (auto It = SurfaceRenderData.Renderables.begin(); It != SurfaceRenderData.Renderables.end(); It++)
        {
            ShaderSystem::BindByID(It->first);
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

                    MaterialSystem::ApplyInstanceProperties(Object.MaterialInstance);
                    MaterialSystem::ApplyLocalProperties(Object.MaterialInstance, Object.ModelMatrix);

                    RendererData.Backend->DrawGeometryData(Object.GeometryID);
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

bool RendererFrontend::BeginMainRenderpass()
{
    return RendererData.Backend->BeginFrame();
}

bool RendererFrontend::EndMainRenderpass()
{
    return RendererData.Backend->EndFrame();
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

void RendererFrontend::ApplyGlobalShaderProperties(Shader* ActiveShader)
{
    RendererData.Backend->ApplyGlobalShaderProperties(ActiveShader);
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

RenderSurfaceT RendererFrontend::CreateRenderSurface(const char* Name, uint32 Width, uint32 Height, bool HasDepth)
{
    RenderSurfaceT Surface = {
        .DebugName = Name,
        .Width = Width,
        .Height = Height,
    };

    Surface.ColorPassTexture = ResourceManager::Ptr->CreateTexture({
        .DebugName = "ColorPassTexture",
        .Dimensions = { (float32)Width, (float32)Height, 1, 4 },
        .Format = Format::BGRA8_UNORM,
        .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT | TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        .Sampler = {
            .WrapModeU = TextureWrapMode::ClampToEdge,
            .WrapModeV = TextureWrapMode::ClampToEdge,
            .WrapModeW = TextureWrapMode::ClampToEdge,
            .Compare = CompareOp::Always,
        },
    });

    if (HasDepth)
    {
        Surface.DepthPassTexture = ResourceManager::Ptr->CreateTexture({
            .DebugName = "DepthPassTexture",
            .Dimensions = { (float32)Width, (float32)Height, 1, 4 },
            .Format = ResourceManager::Ptr->GetPhysicalDeviceFormatSpecs().DepthBufferFormat,
            .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT,
            .Sampler = {
                .WrapModeU = TextureWrapMode::ClampToEdge,
                .WrapModeV = TextureWrapMode::ClampToEdge,
                .WrapModeW = TextureWrapMode::ClampToEdge,
                .Compare = CompareOp::Always,
            },
        });
    }

    Surface.RenderPass = ResourceManager::Ptr->CreateRenderPass({
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

    for (int i = 0; i < 3; i++)
    {
        Surface.CmdBuffers[i] = ResourceManager::Ptr->CreateCommandBuffer({
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

    KDEBUG("Viewport size set to = %d, %d", Width, Height);

    // Delete the old color pass and depth pass
    ResourceManager::Ptr->DestroyRenderPass(Surface.RenderPass);
    ResourceManager::Ptr->DestroyTexture(Surface.ColorPassTexture);
    if (Surface.DepthPassTexture)
    {
        ResourceManager::Ptr->DestroyTexture(Surface.DepthPassTexture);
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

}