#include "kraft_renderer_frontend.h"

#include <core/kraft_base_includes.h>
#include <containers/kraft_containers_includes.h>
#include <renderer/kraft_renderer_includes.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>

#include <kraft_types.h>

// TODO (amn): REMOVE
#include <renderer/kraft_renderer_types.h>
#include <world/kraft_world_includes.h>

#include <shaderfx/kraft_shaderfx_types.h>

#include <platform/kraft_platform_includes.h>

#include <resources/kraft_resource_types.h>

namespace kraft {
r::RendererFrontend* g_Renderer = nullptr;
r::GPUDevice*        g_Device = nullptr;
} // namespace kraft

namespace kraft::r {

using Renderables = FlatHashMap<ResourceID, Array<Renderable>>;

struct RenderDataT
{
    RenderSurface Surface;
    Renderables   Renderables;
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

    Handle<Buffer> vertex_buffer;
    Handle<Buffer> index_buffer;
    u32            current_vertex_buffer_offset;
    u32            current_index_buffer_offset;
    Handle<Buffer> global_ubo_buffer;
    Handle<Buffer> materials_gpu_buffer;
    Handle<Buffer> materials_staging_buffer[3];
} renderer_data_internal;

void RendererFrontend::Init()
{
    const u64 vertex_buffer_size = sizeof(Vertex3D) * 1024 * 256;
    renderer_data_internal.vertex_buffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalVertexBuffer",
        .Size = vertex_buffer_size,
        .UsageFlags = BUFFER_USAGE_FLAGS_STORAGE_BUFFER |
                      BUFFER_USAGE_FLAGS_TRANSFER_DST, // | BufferUsageFlags::BUFFER_USAGE_FLAGS_SHADER_DEVICE_ADDRESS,
        .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        // .MemoryAllocationFlags = MemoryAllocateFlags::MEMORY_ALLOCATE_DEVICE_ADDRESS,
    });

    const u64 index_buffer_size = sizeof(u32) * 1024 * 256;
    renderer_data_internal.index_buffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalIndexBuffer",
        .Size = index_buffer_size,
        .UsageFlags = BUFFER_USAGE_FLAGS_TRANSFER_DST | BUFFER_USAGE_FLAGS_INDEX_BUFFER,
        .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
    });

    renderer_data_internal.current_vertex_buffer_offset = 0;
    renderer_data_internal.current_index_buffer_offset = 0;

    u64 MaterialsBufferSize = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;
    renderer_data_internal.materials_gpu_buffer = ResourceManager->CreateBuffer({
        .Size = MaterialsBufferSize,
        .UsageFlags =
            BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER | BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .SharingMode = SharingMode::Exclusive,
    });

    for (int i = 0; i < KRAFT_C_ARRAY_SIZE(renderer_data_internal.materials_staging_buffer); i++)
    {
        renderer_data_internal.materials_staging_buffer[i] = ResourceManager->CreateBuffer({
            .Size = MaterialsBufferSize,
            .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_SRC,
            .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE |
                                   MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
            .SharingMode = SharingMode::Exclusive,
            .MapMemory = true,
        });
    }

    renderer_data_internal.global_ubo_buffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalUBO",
        .Size = sizeof(GlobalShaderData),
        .UsageFlags =
            BufferUsageFlags::BUFFER_USAGE_FLAGS_TRANSFER_DST | BufferUsageFlags::BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE |
                               MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT |
                               MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .MapMemory = true,
    });
}

struct __DrawData
{
    Mat4f Model;
    vec2  MousePosition;
    u32   EntityId;
    u32   MaterialIdx;
    // u64   vertex_buffer_address;
} DummyDrawData;

void RendererFrontend::DrawSingle(Shader* shader, GlobalShaderData* ubo, u32 geometry_id)
{
    KASSERT(renderer_data_internal.current_frame_index >= 0 && renderer_data_internal.current_frame_index < 3);

    MemCpy(
        (void*)ResourceManager->GetBufferData(renderer_data_internal.global_ubo_buffer),
        (void*)ubo,
        sizeof(GlobalShaderData)
    );

    renderer_data_internal.backend->UseShader(shader, 0);
    renderer_data_internal.backend->ApplyGlobalShaderProperties(
        shader,
        renderer_data_internal.global_ubo_buffer,
        renderer_data_internal.materials_gpu_buffer,
        renderer_data_internal.vertex_buffer,
        renderer_data_internal.index_buffer
    );

    DummyDrawData.Model = ScaleMatrix(vec3{ 1920.0f * 1.2f, 945.0f * 1.2f, 1.0f });
    DummyDrawData.MaterialIdx = 0;
    renderer_data_internal.backend->ApplyLocalShaderProperties(shader, &DummyDrawData);

    // TODO (amn): DrawSingle is unused, update to pass GeometryDrawData properly
    renderer_data_internal.backend->DrawGeometryData({});
}

void RendererFrontend::Draw(GlobalShaderData* global_ubo)
{
    KASSERT(renderer_data_internal.current_frame_index >= 0 && renderer_data_internal.current_frame_index < 3);

    MemCpy(
        (void*)ResourceManager->GetBufferData(renderer_data_internal.global_ubo_buffer),
        (void*)global_ubo,
        sizeof(GlobalShaderData)
    );

    for (auto It = renderer_data_internal.renderables.begin(); It != renderer_data_internal.renderables.end(); It++)
    {
        Shader* shader = ShaderSystem::BindByID(It->first);
        if (!shader)
        {
            It->second.Clear();
            continue;
        }

        renderer_data_internal.backend->ApplyGlobalShaderProperties(
            shader,
            renderer_data_internal.global_ubo_buffer,
            renderer_data_internal.materials_gpu_buffer,
            renderer_data_internal.vertex_buffer,
            renderer_data_internal.index_buffer
        );

        auto& objects = It->second;
        u64   count = objects.Size();
        if (!count)
            continue;

        for (int i = 0; i < count; i++)
        {
            Renderable object = objects[i];

            DummyDrawData.Model = object.ModelMatrix;
            DummyDrawData.MaterialIdx = object.MaterialInstance->ID;
            // DummyDrawData.MousePosition = Surface.RelativeMousePosition;
            DummyDrawData.EntityId = object.EntityId;
            renderer_data_internal.backend->ApplyLocalShaderProperties(shader, &DummyDrawData);
            renderer_data_internal.backend->DrawGeometryData(object.DrawData);
        }

        objects.Clear();

        ShaderSystem::Unbind();
    }
}

void RendererFrontend::OnResize(int width, int height)
{
    KASSERT(renderer_data_internal.backend);
    renderer_data_internal.backend->OnResize(width, height);
}

void RendererFrontend::PrepareFrame()
{
    r::ResourceManager->EndFrame(0);
    renderer_data_internal.current_frame_index = renderer_data_internal.backend->PrepareFrame();

    // Upload all the materials
    u8* buffer_data = ResourceManager->GetBufferData(
        renderer_data_internal.materials_staging_buffer[renderer_data_internal.current_frame_index]
    );
    auto materials = MaterialSystem::GetMaterialsBuffer();
    u64  materials_buffer_size = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;

    MemCpy(buffer_data, materials, materials_buffer_size);

    ResourceManager->UploadBuffer({
        .DstBuffer = renderer_data_internal.materials_gpu_buffer,
        .SrcBuffer = renderer_data_internal.materials_staging_buffer[renderer_data_internal.current_frame_index],
        .SrcSize = materials_buffer_size,
        .DstOffset = 0,
        .SrcOffset = 0,
    });

    // Get all the dirty textures and create descriptor sets for them
    auto dirty_textures = TextureSystem::GetDirtyTextures();
    if (dirty_textures.Length > 0)
    {
        renderer_data_internal.backend->UpdateTextures(dirty_textures.Data(), dirty_textures.Length);
        TextureSystem::ClearDirtyTextures();
    }
}

bool RendererFrontend::DrawSurfaces()
{
    KASSERTM(renderer_data_internal.current_frame_index >= 0, "Did you forget to call Renderer.PrepareFrame()?");

    GlobalShaderData global_shader_data = {};
    global_shader_data.Projection = this->Camera->ProjectionMatrix;
    // global_shader_data.View = this->Camera->GetViewMatrix();
    static vec3 camera_position = vec3{ -5.0f, 0.0f, -10.0f };
    static vec3 camera_rotation = vec3{ 0.0f, 0.0f, 0.0f };
    global_shader_data.View = RotationMatrixFromEulerAngles(camera_rotation) * TranslationMatrix(camera_position);
    global_shader_data.CameraPosition = this->Camera->Position;

    // u64 start = Platform::TimeNowNS();
    for (int i = 0; i < renderer_data_internal.surfaces.Length; i++)
    {
        RenderDataT&   surface_render_data = renderer_data_internal.surfaces[i];
        RenderSurface& surface = surface_render_data.Surface;

        if (surface.World && surface.World->GlobalLight != EntityHandleInvalid)
        {
            Entity GlobalLight = surface.World->GetEntity(surface.World->GlobalLight);
            global_shader_data.GlobalLightColor = GlobalLight.GetComponent<LightComponent>().LightColor;
            global_shader_data.GlobalLightPosition = GlobalLight.GetComponent<TransformComponent>().Position;
        }

        MemCpy(
            (void*)ResourceManager->GetBufferData(surface.GlobalUBO),
            (void*)&global_shader_data,
            sizeof(global_shader_data)
        );

        surface.Begin();
        for (auto it = surface_render_data.Renderables.begin(); it != surface_render_data.Renderables.end(); it++)
        {
            Shader* current_shader = ShaderSystem::BindByID(it->first);
            if (!current_shader)
            {
                it->second.Clear();
                continue;
            }

            renderer_data_internal.backend->ApplyGlobalShaderProperties(
                current_shader,
                surface.GlobalUBO,
                renderer_data_internal.materials_gpu_buffer,
                renderer_data_internal.vertex_buffer,
                renderer_data_internal.index_buffer
            );

            auto& objects = it->second;
            u64   count = objects.Size();
            if (!count)
                continue;

            for (int i = 0; i < count; i++)
            {
                Renderable object = objects[i];

                DummyDrawData.Model = object.ModelMatrix;
                DummyDrawData.MaterialIdx = object.MaterialInstance->ID;
                DummyDrawData.MousePosition = surface.RelativeMousePosition;
                DummyDrawData.EntityId = object.EntityId;
                renderer_data_internal.backend->ApplyLocalShaderProperties(current_shader, &DummyDrawData);

                renderer_data_internal.backend->DrawGeometryData(object.DrawData);
            }

            objects.Clear();

            ShaderSystem::Unbind();
        }
        surface.End();
    }
    // u64 end = kraft::Platform::TimeNowNS();

    // KDEBUG("Render complete in: %f ms", f64(End - Start) / 1000000.0f);

    renderer_data_internal.surfaces.Clear();

    return true;
}

bool RendererFrontend::AddRenderable(const Renderable& renderable)
{
    Renderables* renderables = nullptr;
    if (renderer_data_internal.active_surface != -1)
    {
        renderables = &renderer_data_internal.surfaces[renderer_data_internal.active_surface].Renderables;
    }
    else
    {
        renderables = &renderer_data_internal.renderables;
    }

    ResourceID key = renderable.MaterialInstance->Shader->ID;
    auto       renderable_it = renderables->find(key);
    if (renderable_it == renderables->end())
    {
        auto pair = renderables->insert_or_assign(key, Array<Renderable>());
        auto it = pair.first;
        it->second.Reserve(1024);
        it->second.Push(renderable);
    }
    else
    {
        renderable_it->second.Push(renderable);
    }

    return true;
}

bool RendererFrontend::AddRenderable(SpriteBatch* batch)
{
    Renderable renderable = {
        .ModelMatrix = mat4(Identity),
        .MaterialInstance = batch->material,
        .DrawData = batch->geometry->DrawData,
    };

    return g_Renderer->AddRenderable(renderable);
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

void RendererFrontend::CreateRenderPipeline(Shader* Shader)
{
    renderer_data_internal.backend->CreateRenderPipeline(Shader);
}

void RendererFrontend::DestroyRenderPipeline(Shader* Shader)
{
    renderer_data_internal.backend->DestroyRenderPipeline(Shader);
}

void RendererFrontend::UseShader(const Shader* Shader, u32 variant_index)
{
    renderer_data_internal.backend->UseShader(Shader, variant_index);
}

void RendererFrontend::DrawGeometry(const GeometryDrawData& draw_data)
{
    renderer_data_internal.backend->DrawGeometryData(draw_data);
}

void RendererFrontend::ApplyGlobalShaderProperties(
    Shader*        shader,
    Handle<Buffer> ubo_buffer,
    Handle<Buffer> materials_buffer,
    Handle<Buffer> vertex_buffer,
    Handle<Buffer> index_buffer
)
{
    renderer_data_internal.backend->ApplyGlobalShaderProperties(
        shader, ubo_buffer, materials_buffer, vertex_buffer, index_buffer
    );
}

void RendererFrontend::ApplyLocalShaderProperties(Shader* ActiveShader, void* Data)
{
    renderer_data_internal.backend->ApplyLocalShaderProperties(ActiveShader, Data);
}

bool RendererFrontend::CreateGeometry(
    Geometry*   geometry,
    u32         vertex_count,
    const void* vertices,
    u32         vertex_size,
    u32         index_count,
    const void* indices,
    const u32   index_size
)
{
    u32 vertex_buffer_offset = renderer_data_internal.current_vertex_buffer_offset;
    u32 index_buffer_offset = renderer_data_internal.current_index_buffer_offset;

    renderer_data_internal.current_vertex_buffer_offset += vertex_size * vertex_count;
    renderer_data_internal.current_index_buffer_offset += index_size * index_count;

    geometry->DrawData.IndexCount = index_count;
    geometry->DrawData.IndexBufferOffset = index_buffer_offset;
    geometry->DrawData.VertexOffset = vertex_buffer_offset / vertex_size;

    return renderer_data_internal.backend->CreateGeometry({
        .VertexBuffer = renderer_data_internal.vertex_buffer,
        .VertexBufferOffset = vertex_buffer_offset,
        .VertexCount = vertex_count,
        .Vertices = vertices,
        .VertexSize = vertex_size,
        .IndexBuffer = renderer_data_internal.index_buffer,
        .IndexBufferOffset = index_buffer_offset,
        .IndexCount = index_count,
        .Indices = indices,
        .IndexSize = index_size,
    });
}

bool RendererFrontend::UpdateGeometry(
    Geometry*   geometry,
    u32         vertex_count,
    const void* vertices,
    u32         vertex_size,
    u32         index_count,
    const void* indices,
    const u32   index_size
)
{
    // Re-upload to the same offsets
    u32 vertex_buffer_offset = geometry->DrawData.VertexOffset * vertex_size;
    u32 index_buffer_offset = geometry->DrawData.IndexBufferOffset;

    geometry->DrawData.IndexCount = index_count;

    return renderer_data_internal.backend->UpdateGeometry({
        .VertexBuffer = renderer_data_internal.vertex_buffer,
        .VertexBufferOffset = vertex_buffer_offset,
        .VertexCount = vertex_count,
        .Vertices = vertices,
        .VertexSize = vertex_size,
        .IndexBuffer = renderer_data_internal.index_buffer,
        .IndexBufferOffset = index_buffer_offset,
        .IndexCount = index_count,
        .Indices = indices,
        .IndexSize = index_size,
    });
}

RenderSurface RendererFrontend::CreateRenderSurface(
    String8 name,
    u32     width,
    u32     height,
    bool    has_color,
    bool    has_depth,
    bool    depth_sample
)
{
    RenderSurface surface = {
        .DebugName = name,
        .VariantName = name,
        .Width = width,
        .Height = height,
    };

    RenderPassDescription description = {
        .DebugName = name.str,
        .Dimensions = { (f32)width, (f32)height, 0.f, 0.f },
        .Layout = {
            .Subpasses = {
                {
                    .DepthTarget = has_depth,
                },
            },
        },
    };

    if (has_color)
    {
        surface.ColorPassTexture = ResourceManager->CreateTexture({
            .DebugName = "ColorPassTexture",
            .Dimensions = { (f32)width, (f32)height, 1, 4 },
            .Format = Format::BGRA8_UNORM,
            .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT |
                     TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        });

        description.ColorTargets = {
            {
                .Texture = surface.ColorPassTexture,
                .NextUsage = TextureLayout::Sampled,
                .ClearColor = { 0.10f, 0.10f, 0.10f, 1.0f },
            },
        };

        description.Layout.Subpasses[0].ColorTargetSlots = { 0 };
    }

    if (has_depth)
    {
        u64 usage_flags = TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT;
        if (depth_sample)
        {
            usage_flags |= TEXTURE_USAGE_FLAGS_SAMPLED;
        }

        surface.DepthPassTexture = ResourceManager->CreateTexture({
            .DebugName = "DepthPassTexture",
            .Dimensions = { (f32)width, (f32)height, 1, 4 },
            .Format = ResourceManager->GetPhysicalDeviceFormatSpecs().DepthBufferFormat,
            .Usage = usage_flags,
        });

        description.DepthTarget = {
            .Texture = surface.DepthPassTexture,
            .NextUsage = depth_sample ? TextureLayout::Sampled : TextureLayout::DepthStencil,
            .Depth = 1.0f,
            .Stencil = 0,
        };
    }

    surface.TextureSampler = ResourceManager->CreateTextureSampler({
        .WrapModeU = TextureWrapMode::ClampToEdge,
        .WrapModeV = TextureWrapMode::ClampToEdge,
        .WrapModeW = TextureWrapMode::ClampToEdge,
        .Compare = CompareOp::Never,
    });

    surface.RenderPass = ResourceManager->CreateRenderPass(description);
    surface.GlobalUBO = ResourceManager->CreateBuffer({
        .DebugName = name.str,
        .Size = sizeof(GlobalShaderData),
        .UsageFlags = BUFFER_USAGE_FLAGS_TRANSFER_DST | BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
        .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MEMORY_PROPERTY_FLAGS_HOST_COHERENT |
                               MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .MapMemory = true,
    });

    for (int i = 0; i < 3; i++)
    {
        surface.CmdBuffers[i] = ResourceManager->CreateCommandBuffer({
            .DebugName = name.str,
            .Primary = true,
        });
    }

    return surface;
}

void RendererFrontend::BeginRenderSurface(const RenderSurface& surface)
{
    renderer_data_internal.surfaces.Push({
        .Surface = surface,
    });

    renderer_data_internal.active_surface = renderer_data_internal.surfaces.Length - 1;
}

RenderSurface RendererFrontend::ResizeRenderSurface(RenderSurface& surface, u32 width, u32 height)
{
    if (width == 0 || height == 0)
    {
        return surface;
    }

    //Width *= 0.5f;
    //Height *= 0.5f;

    KDEBUG("Viewport size set to = %d, %d", width, height);

    // Delete the old color pass and depth pass
    ResourceManager->DestroyRenderPass(surface.RenderPass);
    ResourceManager->DestroyTexture(surface.ColorPassTexture);
    if (surface.DepthPassTexture)
    {
        ResourceManager->DestroyTexture(surface.DepthPassTexture);
    }

    // this->CreateCommandBuffers();
    surface = this->CreateRenderSurface(
        surface.DebugName, width, height, surface.DepthPassTexture != Handle<Texture>::Invalid()
    );
    return surface;
}

void RendererFrontend::EndRenderSurface(const RenderSurface& suface)
{
    renderer_data_internal.active_surface = -1;
}

void RendererFrontend::CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx)
{
    renderer_data_internal.backend->CmdSetCustomBuffer(shader, buffer, set_idx, binding_idx);
}

void RenderSurface::Begin()
{
    renderer_data_internal.backend->BeginSurface(this);

    // Set the active shader variant for this surface
    ShaderSystem::SetActiveVariant(this->VariantName);
}

void RenderSurface::End()
{
    renderer_data_internal.backend->EndSurface(this);

    // Reset active variant
    ShaderSystem::SetActiveVariant({});
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
    KASSERTM(
        g_Renderer->Settings->Backend != RendererBackendType::RENDERER_BACKEND_TYPE_NONE,
        "No renderer backend specified"
    );

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
        renderer_data_internal.backend->ApplyGlobalShaderProperties =
            VulkanRendererBackend::ApplyGlobalShaderProperties;
        renderer_data_internal.backend->ApplyLocalShaderProperties = VulkanRendererBackend::ApplyLocalShaderProperties;
        renderer_data_internal.backend->UpdateTextures = VulkanRendererBackend::UpdateTextures;
        renderer_data_internal.backend->CreateGeometry = VulkanRendererBackend::CreateGeometry;
        renderer_data_internal.backend->UpdateGeometry = VulkanRendererBackend::UpdateGeometry;
        renderer_data_internal.backend->DrawGeometryData = VulkanRendererBackend::DrawGeometryData;

        // Render Passes
        renderer_data_internal.backend->BeginSurface = VulkanRendererBackend::BeginSurface;
        renderer_data_internal.backend->EndSurface = VulkanRendererBackend::EndSurface;

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

SpriteBatch* CreateSpriteBatch(ArenaAllocator* arena, u16 batch_size)
{
    u32          vertex_count = batch_size * 4;
    u32          index_count = batch_size * 6;
    SpriteBatch* batch = ArenaPush(arena, SpriteBatch);
    batch->vertices = ArenaPushArray(arena, Vertex2D, vertex_count);
    batch->indices = ArenaPushArray(arena, u32, index_count);

    // We will write the indices once
    for (u16 i = 0; i < batch_size; i++)
    {
        u32 base = i * 4;
        batch->indices[i * 6 + 0] = base + 0;
        batch->indices[i * 6 + 1] = base + 1;
        batch->indices[i * 6 + 2] = base + 2;
        batch->indices[i * 6 + 3] = base + 2;
        batch->indices[i * 6 + 4] = base + 3;
        batch->indices[i * 6 + 5] = base + 0;
    }

    // Now upload everythang!
    batch->geometry = GeometrySystem::AcquireGeometryWithData({
        .VertexCount = vertex_count,
        .IndexCount = index_count,
        .VertexSize = sizeof(Vertex2D),
        .IndexSize = sizeof(u32),
        .Vertices = batch->vertices,
        .Indices = batch->indices,
    });
    batch->quad_count = 0;
    batch->max_quad_count = batch_size;

    return batch;
}

void BeginSpriteBatch(SpriteBatch* batch, Material* material)
{
    batch->material = material;
    batch->quad_count = 0;
}

bool DrawQuad(SpriteBatch* batch, vec2 position, vec2 size, vec2 uv_min, vec2 uv_max, vec4 color)
{
    if (batch->quad_count == batch->max_quad_count)
    {
        KWARN("[SpriteBatch::DrawQuad]: Batch is already full");
        return false;
    }

    batch->vertices[batch->quad_count * 4 + 0] = Vertex2D{
        .Position = { position.x + size.x, position.y + size.y },
        .UV = { uv_max.x, uv_max.y },
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 1] = Vertex2D{
        .Position = { position.x + size.x, position.y },
        .UV = { uv_max.x, uv_min.y },
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 2] = Vertex2D{
        .Position = { position.x, position.y },
        .UV = { uv_min.x, uv_min.y },
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 3] = Vertex2D{
        .Position = { position.x, position.y + size.y },
        .UV = { uv_min.x, uv_max.y },
        .Color = color,
    };

    batch->quad_count += 1;

    return true;
}

bool DrawQuad(SpriteBatch* batch, vec2 p0, vec2 p1, vec2 p2, vec2 p3, vec2 uv_min, vec2 uv_max, vec4 color)
{
    if (batch->quad_count == batch->max_quad_count)
    {
        KWARN("[SpriteBatch::DrawQuad]: Batch is already full");
        return false;
    }

    batch->vertices[batch->quad_count * 4 + 0] = Vertex2D{
        .Position = p0,
        .UV = { uv_max.x, uv_max.y },
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 1] = Vertex2D{
        .Position = p1,
        .UV = { uv_max.x, uv_min.y },
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 2] = Vertex2D{
        .Position = p2,
        .UV = { uv_min.x, uv_min.y },
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 3] = Vertex2D{
        .Position = p3,
        .UV = { uv_min.x, uv_max.y },
        .Color = color,
    };

    batch->quad_count += 1;

    return true;
}

void EndSpriteBatch(SpriteBatch* batch)
{
    g_Renderer->UpdateGeometry(
        batch->geometry,
        batch->quad_count * 4,
        batch->vertices,
        sizeof(Vertex2D),
        0,
        nullptr, // We don't want to update indices again
        sizeof(u32)
    );

    batch->geometry->DrawData.IndexCount = batch->quad_count * 6;
}

//
// SpriteBatch
//

} // namespace kraft::r