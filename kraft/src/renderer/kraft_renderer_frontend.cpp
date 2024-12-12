#include "kraft_renderer_frontend.h"

#include <containers/kraft_hashmap.h>
#include <core/kraft_asserts.h>
#include <core/kraft_engine.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <renderer/kraft_camera.h>
#include <renderer/kraft_renderer_backend.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <renderer/kraft_resource_manager.h>
#include <world/kraft_world.h>
#include <world/kraft_entity.h>

#include <resources/kraft_resource_types.h>
#include <renderer/shaderfx/kraft_shaderfx_types.h>

namespace kraft::renderer {

using RenderablesT = FlatHashMap<ResourceID, Array<Renderable>>;

struct RenderDataT
{
    RenderSurfaceT Surface;
    RenderablesT   Renderables;
};

struct RendererDataT
{
    kraft::Block        BackendMemory;
    RendererBackendType Type = RendererBackendType::RENDERER_BACKEND_TYPE_NONE;
    RenderablesT        Renderables;
    Array<RenderDataT>  RenderSurfaces;
    RendererBackend*    Backend = nullptr;
    int                 ActiveSurface = -1;
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

bool RendererFrontend::DrawFrame()
{
    GlobalUniformData GlobalShaderData = {};
    GlobalShaderData.Projection = this->Camera->ProjectionMatrix;
    GlobalShaderData.View = this->Camera->GetViewMatrix();
    GlobalShaderData.CameraPosition = this->Camera->Position;

    int CurrentFrame = RendererData.Backend->PrepareFrame();
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

        RendererData.Backend->BeginRenderPass(Surface.CmdBuffers[CurrentFrame], Surface.RenderPass);
        for (auto It = SurfaceRenderData.Renderables.begin(); It != SurfaceRenderData.Renderables.end(); It++)
        {
            auto&  Objects = It->second;
            uint64 Count = Objects.Size();
            if (!Count)
                continue;
            ShaderSystem::Bind(Objects[0].MaterialInstance->Shader);
            ShaderSystem::ApplyGlobalProperties(GlobalShaderData);
            for (int i = 0; i < Count; i++)
            {
                Renderable Object = Objects[i];
                ShaderSystem::SetMaterialInstance(Object.MaterialInstance);

                MaterialSystem::ApplyInstanceProperties(Object.MaterialInstance);
                MaterialSystem::ApplyLocalProperties(Object.MaterialInstance, Object.ModelMatrix);

                RendererData.Backend->DrawGeometryData(Object.GeometryID);
            }
            ShaderSystem::Unbind();

            Objects.Clear();
        }
        RendererData.Backend->EndRenderPass(Surface.CmdBuffers[CurrentFrame], Surface.RenderPass);
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
    if (!Renderables->contains(Key))
    {
        (*Renderables)[Key] = Array<Renderable>();
        (*Renderables)[Key].Reserve(1024);
    }

    (*Renderables)[Key].Push(Object);

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

bool RendererFrontend::CreateGeometry(
    Geometry*    Geometry,
    uint32       VertexCount,
    const void*  Vertices,
    uint32       VertexSize,
    uint32       IndexCount,
    const void*  Indices,
    const uint32 IndexSize
)
{
    return RendererData.Backend->CreateGeometry(Geometry, VertexCount, Vertices, VertexSize, IndexCount, Indices, IndexSize);
}

void RendererFrontend::DestroyGeometry(Geometry* Geometry)
{
    RendererData.Backend->DestroyGeometry(Geometry);
}

RenderSurfaceT RendererFrontend::CreateRenderSurface(const char* Name, uint32 Width, uint32 Height, bool HasDepth)
{
    static int     Index = 0; // TODO: Temp stuff
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

}