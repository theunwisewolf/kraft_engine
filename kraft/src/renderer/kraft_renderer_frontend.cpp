#include "kraft_renderer_frontend.h"

#include "kraft_includes.h"

#include <kraft_types.h>

#include "radsort/radsort.h"

#define KRAFT_DYNAMIC_GEOMETRY_USE_HOST_VISIBLE_BUFFER 1

namespace kraft {
r::RendererFrontend* g_Renderer = nullptr;
r::GPUDevice* g_Device = nullptr;
} // namespace kraft

namespace kraft::r {

struct ResourceManager* ResourceManager = nullptr;

static RendererFrameStats& CurrentFrameStats() {
    return g_Renderer->Stats.current_frame;
}

static void BeginStatsFrame(RendererFrontend* renderer) {
#if 0
    if (renderer->Stats.current_frame.frame_id != 0) {
        renderer->Stats.last_completed_frame = renderer->Stats.current_frame;

        // Periodic slug stats dump every 120 frames
        if (renderer->Stats.current_frame.frame_id % 120 == 0) {
            const RendererSlugTextStats& s = renderer->Stats.current_frame.slug_text;
            if (s.call_count > 0) {
                KINFO("=== SlugRenderText Stats (frame %llu) ===", renderer->Stats.current_frame.frame_id);
                KINFO("  Calls: %u | Quads: %u | TextBytes: %llu", s.call_count, s.quad_count, s.text_byte_count);
                KINFO("  Total:          %.3f ms", s.total_ms);
                KINFO("  BuildGeometry:  %.3f ms (glyph_resolve: %.3f, kerning: %.3f, quad_build: %.3f)", s.build_geometry_ms, s.glyph_resolve_ms, s.kerning_ms, s.quad_build_ms);
                KINFO("  AllocDynGeo:    %.3f ms", s.allocate_dynamic_geometry_ms);
                KINFO("  VertexMemcpy:   %.3f ms (%llu bytes)", s.vertex_memcpy_ms, s.vertex_bytes_generated);
                KINFO("  IndexMemcpy:    %.3f ms (%llu bytes)", s.index_memcpy_ms, s.index_bytes_generated);
                KINFO("  MaterialUpdate: %.3f ms", s.material_update_ms);
                KINFO("=======================================");
            }

            const RendererSurfaceStats& surf = renderer->Stats.current_frame.surfaces;
            KINFO("=== Surface Stats (frame %llu) ===", renderer->Stats.current_frame.frame_id);
            KINFO(
                "  Begin: %.3f ms (%u) | Submit: %.3f ms (%u) | End: %.3f ms (%u)", surf.begin_total_ms, surf.begin_count, surf.submit_total_ms, surf.submit_count, surf.end_total_ms, surf.end_count
            );
            KINFO("  Sort: %.3f | InitShaderBind: %.3f | GlobalProps: %.3f (%u)", surf.sort_ms, surf.initial_shader_bind_ms, surf.global_properties_ms, surf.global_properties_count);
            KINFO("  PipelineSwitch: %.3f (%u) | CustomBindGroup: %.3f (%u)", surf.pipeline_switch_ms, surf.pipeline_bind_count, surf.custom_bind_group_ms, surf.custom_bind_group_bind_count);
            KINFO("  LocalProps: %.3f (%u) | DrawGeometry: %.3f (%u)", surf.local_properties_ms, surf.local_properties_count, surf.draw_geometry_ms, surf.draw_call_count);
            KINFO("  BeginSurface: %.3f | EndSurface: %.3f | ResetVariant: %.3f", surf.begin_surface_ms, surf.end_surface_ms, surf.reset_active_variant_ms);

            const RendererDynamicGeometryStats& dg = renderer->Stats.current_frame.dynamic_geometry;
            KINFO("=== Dynamic Geometry Stats ===");
            KINFO("  AllocTotal: %.3f ms (%u allocs) | UploadTotal: %.3f ms", dg.allocation_total_ms, dg.allocation_count, dg.upload_total_ms);
            KINFO("  VertexRequested: %llu bytes | IndexRequested: %llu bytes", dg.vertex_bytes_requested, dg.index_bytes_requested);
            KINFO(
                "  VertexUpload: %.3f ms (%u calls, %u blocks, %u touched)", dg.vertex_upload.total_ms, dg.vertex_upload.call_count, dg.vertex_upload.block_count, dg.vertex_upload.touched_block_count
            );
            KINFO("  IndexUpload: %.3f ms (%u calls, %u blocks, %u touched)", dg.index_upload.total_ms, dg.index_upload.call_count, dg.index_upload.block_count, dg.index_upload.touched_block_count);
            KINFO("=======================================");
            fflush(stdout);
        }
    }
#endif

    renderer->Stats.current_frame = {};
    renderer->Stats.current_frame.frame_id = ++renderer->StatsFrameCounter;
}

// For drawing dynamic geometry, this represents a single "block" in the chain
// of allocated memory blocks
struct DynamicGeometryBlock {
#if KRAFT_DYNAMIC_GEOMETRY_USE_HOST_VISIBLE_BUFFER
    Handle<Buffer> buffer;
    u8* mapped;
#else
    Handle<Buffer> gpu_buffer;
    Handle<Buffer> staging_buffer;
#endif

    u32 used_bytes;
    DynamicGeometryBlock* next;
};

// Dynamic geometry allocator
// Allocates 4 MB blocks, one vertex and one index chain per frame
// Blocks are host-visible so we don't need a staging buffer and grow on demand via a linked-list
struct DynamicGeometryAllocator {
    DynamicGeometryBlock* head = nullptr;
    DynamicGeometryBlock* current = nullptr;

    u32 offset = 0;
    u32 capacity = 0;
    u64 usage_flags = 0;
    u64 memory_flags = 0;

    struct Result {
        u8* ptr;
        Handle<Buffer> buffer;
        u32 byte_offset;
    };

    // Rewind to the head block and reset the offset
    // Doesn't release any memory
    void Reset() {
        if (head) {
            current = head;
        }

        DynamicGeometryBlock* block = head;
        while (block) {
            block->used_bytes = 0;
            block = block->next;
        }

        offset = 0;
    }

    DynamicGeometryBlock* AllocateNewBlock(ArenaAllocator* arena) {
        DynamicGeometryBlock* block = ArenaPush(arena, DynamicGeometryBlock);

#if KRAFT_DYNAMIC_GEOMETRY_USE_HOST_VISIBLE_BUFFER
        Handle<Buffer> buffer = ResourceManager->CreateBuffer({
            .Size = capacity,
            .UsageFlags = usage_flags,
            .MemoryPropertyFlags = memory_flags,
            .MapMemory = true,
        });

        KASSERT(buffer != Handle<Buffer>::Invalid());

        block->buffer = buffer;
        block->mapped = ResourceManager->GetBufferData(buffer);
#else
        block->gpu_buffer = ResourceManager->CreateBuffer({
            .Size = capacity,
            .UsageFlags = usage_flags,
            .MemoryPropertyFlags = memory_flags,
        });

        block->staging_buffer = ResourceManager->CreateBuffer({
            .Size = capacity,
            .UsageFlags = BUFFER_USAGE_FLAGS_TRANSFER_SRC,
            .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
            .MapMemory = true,
        });
#endif

        block->used_bytes = 0;
        block->next = nullptr;

        return block;
    }

    // Bump-allocate 'size' bytes with the given alignment
    // Returns a pointer into mapped GPU memory
    Result Alloc(ArenaAllocator* arena, u32 size, u32 alignment) {
        KASSERTM(size <= capacity, "Single dynamic geometry allocation exceeds block capacity");

        // Ensure we have at least one block
        if (!current) {
            head = current = AllocateNewBlock(arena);
        }

        u32 aligned_offset = (u32)math::AlignUp((u64)offset, (u64)alignment);

        // Doesn't fit in current block? Try the next block
        if (aligned_offset + size > capacity) {
            // Allocate a new block if one isn't available
            if (!current->next) {
                DynamicGeometryBlock* new_block = AllocateNewBlock(arena);
                current->next = new_block;
            }

            current = current->next;
            aligned_offset = 0;
        }

        offset = aligned_offset + size;
        current->used_bytes = offset;

#if KRAFT_DYNAMIC_GEOMETRY_USE_HOST_VISIBLE_BUFFER
        return {
            .ptr = current->mapped + aligned_offset,
            .buffer = current->buffer,
            .byte_offset = aligned_offset,
        };
#else
        u8* buffer_data = ResourceManager->GetBufferData(current->staging_buffer);
        return {
            .ptr = buffer_data + aligned_offset, // We want to copy on the staging buffer
            .buffer = current->gpu_buffer, // But for the final "buffer device address" calculation, we want to use the device local buffer
            .byte_offset = aligned_offset,
        };
#endif
    }
};

struct RendererFrontendPrivate {
    RendererBackend* backend = nullptr;
    ArenaAllocator* arena;
    ArenaAllocator* frame_arena;
    int current_frame_index = -1;

    RenderSurface main_surface;

    // For static geometry
    Handle<Buffer> vertex_buffer;
    Handle<Buffer> index_buffer;
    u32 current_vertex_buffer_offset;
    u32 current_index_buffer_offset;
    Handle<Buffer> global_ubo_buffer;
    Handle<Buffer> materials_gpu_buffer;
    Handle<Buffer> materials_staging_buffer[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];

    // Per-frame dynamic geometry
    DynamicGeometryAllocator dynamic_vertex_buffer_allocator[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
    DynamicGeometryAllocator dynamic_index_buffer_allocator[KRAFT_VULKAN_MAX_SWAPCHAIN_IMAGES];
} renderer_data_internal;

static inline u64 MakeSortKey(u16 render_queue, u16 pipeline_id, u16 material_idx, u16 sequence) {
    return ((u64)render_queue << 48) | ((u64)pipeline_id << 32) | ((u64)material_idx << 16) | (u64)sequence;
}

void RendererFrontend::Init() {
    const u64 vertex_buffer_size = sizeof(Vertex3D) * 1024 * 256;
    renderer_data_internal.vertex_buffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalVertexBuffer",
        .Size = vertex_buffer_size,
        .UsageFlags = BUFFER_USAGE_FLAGS_STORAGE_BUFFER | BUFFER_USAGE_FLAGS_TRANSFER_DST | BUFFER_USAGE_FLAGS_SHADER_DEVICE_ADDRESS,
        .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
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

    u64 materials_buffer_size = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;
    renderer_data_internal.materials_gpu_buffer = ResourceManager->CreateBuffer({
        .Size = materials_buffer_size,
        .UsageFlags = BUFFER_USAGE_FLAGS_STORAGE_BUFFER | BUFFER_USAGE_FLAGS_TRANSFER_DST,
        .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .SharingMode = SharingMode::Exclusive,
    });

    for (int i = 0; i < KRAFT_C_ARRAY_SIZE(renderer_data_internal.materials_staging_buffer); i++) {
        renderer_data_internal.materials_staging_buffer[i] = ResourceManager->CreateBuffer({
            .Size = materials_buffer_size,
            .UsageFlags = BUFFER_USAGE_FLAGS_TRANSFER_SRC,
            .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
            .SharingMode = SharingMode::Exclusive,
            .MapMemory = true,
        });
    }

    renderer_data_internal.global_ubo_buffer = ResourceManager->CreateBuffer({
        .DebugName = "GlobalUBO",
        .Size = sizeof(GlobalShaderData),
        .UsageFlags = BUFFER_USAGE_FLAGS_TRANSFER_DST | BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
        .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MEMORY_PROPERTY_FLAGS_HOST_COHERENT | MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .MapMemory = true,
    });

    // Frame arena for per-frame allocations (draws, draw constants)
    renderer_data_internal.frame_arena = CreateArena({
        .ChunkSize = KRAFT_SIZE_MB(4),
        .Alignment = 16,
    });

#if KRAFT_DYNAMIC_GEOMETRY_USE_HOST_VISIBLE_BUFFER
    const u32 block_capacity = KRAFT_SIZE_MB(4);
    for (int i = 0; i < 3; i++) {
        renderer_data_internal.dynamic_vertex_buffer_allocator[i] = {
            .capacity = block_capacity,
            .usage_flags = BUFFER_USAGE_FLAGS_STORAGE_BUFFER | BUFFER_USAGE_FLAGS_SHADER_DEVICE_ADDRESS,
            .memory_flags = MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
        };

        renderer_data_internal.dynamic_index_buffer_allocator[i] = {
            .capacity = block_capacity,
            .usage_flags = BUFFER_USAGE_FLAGS_INDEX_BUFFER,
            .memory_flags = MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MEMORY_PROPERTY_FLAGS_HOST_COHERENT,
        };
    }
#else
    const u32 block_capacity = KRAFT_SIZE_MB(4);
    for (int i = 0; i < 3; i++) {
        renderer_data_internal.dynamic_vertex_buffer_allocator[i] = {
            .capacity = block_capacity,
            .usage_flags = BUFFER_USAGE_FLAGS_STORAGE_BUFFER | BUFFER_USAGE_FLAGS_TRANSFER_DST | BUFFER_USAGE_FLAGS_SHADER_DEVICE_ADDRESS,
            .memory_flags = MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        };

        renderer_data_internal.dynamic_index_buffer_allocator[i] = {
            .capacity = block_capacity,
            .usage_flags = BUFFER_USAGE_FLAGS_INDEX_BUFFER | BUFFER_USAGE_FLAGS_TRANSFER_DST,
            .memory_flags = MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        };
    }
#endif

    // Main swapchain surface
    renderer_data_internal.main_surface.is_swapchain = true;
    renderer_data_internal.main_surface.global_ubo = renderer_data_internal.global_ubo_buffer;
}

void RendererFrontend::DrawSingle(Shader* shader, GlobalShaderData* ubo, u32 geometry_id) {
    // KASSERT(renderer_data_internal.current_frame_index >= 0 && renderer_data_internal.current_frame_index < 3);

    // MemCpy((void*)ResourceManager->GetBufferData(renderer_data_internal.global_ubo_buffer), (void*)ubo, sizeof(GlobalShaderData));

    // renderer_data_internal.backend->UseShader(shader, 0);
    // renderer_data_internal.backend->ApplyGlobalShaderProperties(
    //     shader, renderer_data_internal.global_ubo_buffer, renderer_data_internal.materials_gpu_buffer, renderer_data_internal.vertex_buffer, renderer_data_internal.index_buffer
    // );

    // DummyDrawData.Model = ScaleMatrix(vec3{1920.0f * 1.2f, 945.0f * 1.2f, 1.0f});
    // DummyDrawData.MaterialIdx = 0;
    // renderer_data_internal.backend->ApplyLocalShaderProperties(shader, &DummyDrawData);

    // // TODO (amn): DrawSingle is unused, update to pass GeometryDrawData properly
    // renderer_data_internal.backend->DrawGeometryData({});
}

void RendererFrontend::OnResize(int width, int height) {
    KASSERT(renderer_data_internal.backend);
    renderer_data_internal.backend->OnResize(width, height);
}

void RendererFrontend::PrepareFrame() {
    BeginStatsFrame(this);
    RendererFrameStats& frame_stats = CurrentFrameStats();
    RendererPrepareFrameStats& prepare_stats = frame_stats.prepare_frame;
    KRAFT_TIMING_STATS_BEGIN(frame_timer);
    KRAFT_TIMING_STATS_BEGIN(prepare_timer);

    {
        KRAFT_TIMING_STATS_BEGIN(resource_end_frame_timer);
        r::ResourceManager->EndFrame(0);
        KRAFT_TIMING_STATS_ACCUM(resource_end_frame_timer, prepare_stats.resource_end_frame_ms);
    }

    {
        KRAFT_TIMING_STATS_BEGIN(backend_prepare_frame_timer);
        renderer_data_internal.current_frame_index = renderer_data_internal.backend->PrepareFrame();
        KRAFT_TIMING_STATS_ACCUM(backend_prepare_frame_timer, prepare_stats.backend_prepare_frame_ms);
    }

    // Upload all the materials if dirty
    if (MaterialSystem::IsDirty()) {
        u8* buffer_data = ResourceManager->GetBufferData(renderer_data_internal.materials_staging_buffer[renderer_data_internal.current_frame_index]);
        auto materials = MaterialSystem::GetMaterialsBuffer();
        u64 materials_buffer_size = this->Settings->MaxMaterials * this->Settings->MaterialBufferSize;

        prepare_stats.materials_uploaded = true;
        {
            KRAFT_TIMING_STATS_BEGIN(material_stage_memcpy_timer);
            MemCpy(buffer_data, materials, materials_buffer_size);
            KRAFT_TIMING_STATS_ACCUM(material_stage_memcpy_timer, prepare_stats.material_stage_memcpy_ms);
        }

        {
            KRAFT_TIMING_STATS_BEGIN(material_upload_timer);
            ResourceManager->UploadBuffer({
                .DstBuffer = renderer_data_internal.materials_gpu_buffer,
                .SrcBuffer = renderer_data_internal.materials_staging_buffer[renderer_data_internal.current_frame_index],
                .SrcSize = materials_buffer_size,
                .DstOffset = 0,
                .SrcOffset = 0,
            });
            KRAFT_TIMING_STATS_ACCUM(material_upload_timer, prepare_stats.material_upload_ms);
        }

        MaterialSystem::SetDirty(false);
    }

    // Get all the dirty textures and create descriptor sets for them
    auto dirty_textures = TextureSystem::GetDirtyTextures();
    prepare_stats.dirty_texture_count = dirty_textures.Length;
    if (dirty_textures.Length > 0) {
        KRAFT_TIMING_STATS_BEGIN(texture_update_timer);
        renderer_data_internal.backend->UpdateTextures(dirty_textures.Data(), dirty_textures.Length);
        TextureSystem::ClearDirtyTextures();
        KRAFT_TIMING_STATS_ACCUM(texture_update_timer, prepare_stats.texture_update_ms);
    }

    // Reset the dynamic geometry allocator for this frame
    int frame = renderer_data_internal.current_frame_index;
    {
        KRAFT_TIMING_STATS_BEGIN(allocator_reset_timer);
        renderer_data_internal.dynamic_vertex_buffer_allocator[frame].Reset();
        renderer_data_internal.dynamic_index_buffer_allocator[frame].Reset();
        KRAFT_TIMING_STATS_ACCUM(allocator_reset_timer, prepare_stats.allocator_reset_ms);
    }

    KRAFT_TIMING_STATS_ACCUM(prepare_timer, prepare_stats.total_ms);
    KRAFT_TIMING_STATS_ACCUM2(frame_timer, frame_stats.total_ms, frame_stats.prepare_frame_ms);
}

DynamicGeometryAllocation RendererFrontend::AllocateDynamicGeometry(u32 vertex_count, u32 vertex_stride, u32 index_count) {
    int frame = renderer_data_internal.current_frame_index;
    KASSERTM(frame >= 0, "Did you forget to call PrepareFrame?");

    RendererDynamicGeometryStats& dynamic_stats = CurrentFrameStats().dynamic_geometry;
    KRAFT_TIMING_STATS_BEGIN(dynamic_alloc_timer);

    u32 vertex_size = vertex_count * vertex_stride;
    u32 index_size = index_count * sizeof(u32);

    u32 vertex_align = (u32)g_Device->min_storage_buffer_alignment;
    u32 index_align = sizeof(u32); // vkCmdBindIndexBuffer requires 4-byte alignment for UINT32

    ArenaAllocator* arena = renderer_data_internal.arena;

    auto vertex_ptr = renderer_data_internal.dynamic_vertex_buffer_allocator[frame].Alloc(arena, vertex_size, vertex_align);
    auto index_ptr = renderer_data_internal.dynamic_index_buffer_allocator[frame].Alloc(arena, index_size, index_align);

    dynamic_stats.allocation_count++;
    dynamic_stats.vertex_bytes_requested += vertex_size;
    dynamic_stats.index_bytes_requested += index_size;
    KRAFT_TIMING_STATS_ACCUM(dynamic_alloc_timer, dynamic_stats.allocation_total_ms);

    return {
        .vertex_buffer = vertex_ptr.buffer,
        .index_buffer = index_ptr.buffer,
        .vertex_byte_offset = vertex_ptr.byte_offset,
        .index_byte_offset = index_ptr.byte_offset,
        .vertex_ptr = vertex_ptr.ptr,
        .index_ptr = index_ptr.ptr,
        .vertex_count = vertex_count,
        .index_count = index_count,
        .vertex_stride = vertex_stride,
    };
}

RenderSurface* RendererFrontend::GetMainSurface() {
    return &renderer_data_internal.main_surface;
}

void RendererFrontend::BeginMainRenderpass() {
    RendererFrameStats& frame_stats = CurrentFrameStats();
    KRAFT_TIMING_STATS_BEGIN(begin_main_renderpass_timer);
    renderer_data_internal.backend->BeginFrame();
    KRAFT_TIMING_STATS_ACCUM2(begin_main_renderpass_timer, frame_stats.total_ms, frame_stats.begin_main_renderpass_ms);
}

void RendererFrontend::EndMainRenderpass() {
    RendererFrameStats& frame_stats = CurrentFrameStats();
    KRAFT_TIMING_STATS_BEGIN(end_main_renderpass_timer);
    renderer_data_internal.backend->EndFrame();
    KRAFT_TIMING_STATS_ACCUM2(end_main_renderpass_timer, frame_stats.total_ms, frame_stats.end_main_renderpass_ms);
}

//
// API
//

void RendererFrontend::CreateRenderPipeline(Shader* Shader) {
    renderer_data_internal.backend->CreateRenderPipeline(Shader);
}

void RendererFrontend::DestroyRenderPipeline(Shader* Shader) {
    renderer_data_internal.backend->DestroyRenderPipeline(Shader);
}

void RendererFrontend::UseShader(const Shader* Shader, u32 variant_index) {
    renderer_data_internal.backend->UseShader(Shader, variant_index);
}

void RendererFrontend::DrawGeometry(const GeometryDrawData& draw_data, Handle<Buffer> index_buffer) {
    renderer_data_internal.backend->DrawGeometryData(draw_data, index_buffer);
}

void RendererFrontend::ApplyGlobalShaderProperties(Shader* shader, Handle<Buffer> ubo_buffer, Handle<Buffer> materials_buffer) {
    renderer_data_internal.backend->ApplyGlobalShaderProperties(shader, ubo_buffer, materials_buffer);
}

void RendererFrontend::ApplyLocalShaderProperties(Shader* ActiveShader, void* Data) {
    renderer_data_internal.backend->ApplyLocalShaderProperties(ActiveShader, Data);
}

bool RendererFrontend::CreateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size) {
    u32 vertex_buffer_offset = renderer_data_internal.current_vertex_buffer_offset;
    u32 index_buffer_offset = renderer_data_internal.current_index_buffer_offset;

    renderer_data_internal.current_vertex_buffer_offset += vertex_size * vertex_count;
    renderer_data_internal.current_index_buffer_offset += index_size * index_count;

    geometry->DrawData.IndexCount = index_count;
    geometry->DrawData.IndexBufferOffset = index_buffer_offset;
    geometry->DrawData.VertexByteOffset = vertex_buffer_offset;

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

bool RendererFrontend::UpdateGeometry(Geometry* geometry, u32 vertex_count, const void* vertices, u32 vertex_size, u32 index_count, const void* indices, const u32 index_size) {
    // Re-upload to the same offsets
    u32 vertex_buffer_offset = geometry->DrawData.VertexByteOffset;
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

RenderSurface RendererFrontend::CreateRenderSurface(String8 name, u32 width, u32 height, bool has_color, bool has_depth, bool depth_sample) {
    RenderSurface surface = {
        .debug_name = name,
        .variant_name = name,
        .width = width,
        .height = height,
    };

    RenderPassDescription description = {
        .DebugName = name.str,
        .Dimensions = {(f32)width, (f32)height, 0.f, 0.f},
        .Layout =
            {
                .Subpasses =
                    {
                        {
                            .DepthTarget = has_depth,
                        },
                    },
            },
    };

    if (has_color) {
        surface.color_pass = ResourceManager->CreateTexture({
            .DebugName = "ColorPassTexture",
            .Dimensions = {(f32)width, (f32)height, 1, 4},
            .Format = Format::BGRA8_UNORM,
            .Usage = TextureUsageFlags::TEXTURE_USAGE_FLAGS_COLOR_ATTACHMENT | TextureUsageFlags::TEXTURE_USAGE_FLAGS_SAMPLED,
        });

        description.ColorTargets = {
            {
                .Texture = surface.color_pass,
                .NextUsage = TextureLayout::Sampled,
                .ClearColor = {0.10f, 0.10f, 0.10f, 1.0f},
            },
        };

        description.Layout.Subpasses[0].ColorTargetSlots = {0};
    }

    if (has_depth) {
        u64 usage_flags = TEXTURE_USAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT;
        if (depth_sample) {
            usage_flags |= TEXTURE_USAGE_FLAGS_SAMPLED;
        }

        surface.depth_pass = ResourceManager->CreateTexture({
            .DebugName = "DepthPassTexture",
            .Dimensions = {(f32)width, (f32)height, 1, 4},
            .Format = ResourceManager->GetPhysicalDeviceFormatSpecs().DepthBufferFormat,
            .Usage = usage_flags,
        });

        description.DepthTarget = {
            .Texture = surface.depth_pass,
            .NextUsage = depth_sample ? TextureLayout::Sampled : TextureLayout::DepthStencil,
            .Depth = 1.0f,
            .Stencil = 0,
        };
    }

    surface.texture_sampler = ResourceManager->CreateTextureSampler({
        .WrapModeU = TextureWrapMode::ClampToEdge,
        .WrapModeV = TextureWrapMode::ClampToEdge,
        .WrapModeW = TextureWrapMode::ClampToEdge,
        .Compare = CompareOp::Never,
    });

#if !KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    surface.render_pass = ResourceManager->CreateRenderPass(description);
#endif

    surface.global_ubo = ResourceManager->CreateBuffer({
        .DebugName = name.str,
        .Size = sizeof(GlobalShaderData),
        .UsageFlags = BUFFER_USAGE_FLAGS_TRANSFER_DST | BUFFER_USAGE_FLAGS_UNIFORM_BUFFER,
        .MemoryPropertyFlags = MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MEMORY_PROPERTY_FLAGS_HOST_COHERENT | MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL,
        .MapMemory = true,
    });

    for (int i = 0; i < 3; i++) {
        surface.cmd_buffers[i] = ResourceManager->CreateCommandBuffer({
            .DebugName = name.str,
            .Primary = true,
        });
    }

    return surface;
}

RenderSurface RendererFrontend::ResizeRenderSurface(RenderSurface& surface, u32 width, u32 height) {
    if (width == 0 || height == 0) {
        return surface;
    }

    //Width *= 0.5f;
    //Height *= 0.5f;

    KDEBUG("Viewport size set to = %d, %d", width, height);

#if !KRAFT_ENABLE_VK_DYNAMIC_RENDERING
    ResourceManager->DestroyRenderPass(surface.render_pass);
#endif

    // Delete the old color pass and depth pass
    ResourceManager->DestroyTexture(surface.color_pass);
    if (surface.depth_pass) {
        ResourceManager->DestroyTexture(surface.depth_pass);
    }

    surface = this->CreateRenderSurface(surface.debug_name, width, height, surface.depth_pass != Handle<Texture>::Invalid());
    return surface;
}

void RendererFrontend::CmdSetCustomBuffer(Shader* shader, Handle<Buffer> buffer, u32 set_idx, u32 binding_idx) {
    renderer_data_internal.backend->CmdSetCustomBuffer(shader, buffer, set_idx, binding_idx);
}

const RendererStats& RendererFrontend::GetStats() const {
    return this->Stats;
}

void RendererFrontend::RecordSlugTextStats(const RendererSlugTextStats& stats) {
    if (this->Stats.current_frame.frame_id == 0) {
        return;
    }

    RendererFrameStats& frame_stats = this->Stats.current_frame;
    RendererSlugTextStats& slug_stats = frame_stats.slug_text;

    frame_stats.total_ms += stats.total_ms;
    slug_stats.total_ms += stats.total_ms;
    slug_stats.build_geometry_ms += stats.build_geometry_ms;
    slug_stats.glyph_resolve_ms += stats.glyph_resolve_ms;
    slug_stats.kerning_ms += stats.kerning_ms;
    slug_stats.quad_build_ms += stats.quad_build_ms;
    slug_stats.allocate_dynamic_geometry_ms += stats.allocate_dynamic_geometry_ms;
    slug_stats.vertex_memcpy_ms += stats.vertex_memcpy_ms;
    slug_stats.index_memcpy_ms += stats.index_memcpy_ms;
    slug_stats.material_update_ms += stats.material_update_ms;
    slug_stats.call_count += stats.call_count;
    slug_stats.quad_count += stats.quad_count;
    slug_stats.text_byte_count += stats.text_byte_count;
    slug_stats.vertex_bytes_generated += stats.vertex_bytes_generated;
    slug_stats.index_bytes_generated += stats.index_bytes_generated;
}

Geometry* RendererFrontend::CreatePlaneGeometry(f32 uv_scale) {
    Vertex3D vertices[4] = {
        {.Position = {-0.5f, 0.0f, -0.5f}, .UV = {0.0f, 0.0f}, .Normal = {0.0f, 1.0f, 0.0f}, .Tangent = {1.0f, 0.0f, 0.0f, 1.0f}},
        {.Position = {0.5f, 0.0f, -0.5f}, .UV = {uv_scale, 0.0f}, .Normal = {0.0f, 1.0f, 0.0f}, .Tangent = {1.0f, 0.0f, 0.0f, 1.0f}},
        {.Position = {0.5f, 0.0f, 0.5f}, .UV = {uv_scale, uv_scale}, .Normal = {0.0f, 1.0f, 0.0f}, .Tangent = {1.0f, 0.0f, 0.0f, 1.0f}},
        {.Position = {-0.5f, 0.0f, 0.5f}, .UV = {0.0f, uv_scale}, .Normal = {0.0f, 1.0f, 0.0f}, .Tangent = {1.0f, 0.0f, 0.0f, 1.0f}},
    };

    u32 indices[6] = {0, 2, 1, 0, 3, 2};

    return GeometrySystem::AcquireGeometryWithData({
        .VertexCount = 4,
        .IndexCount = 6,
        .VertexSize = sizeof(Vertex3D),
        .IndexSize = sizeof(u32),
        .Vertices = vertices,
        .Indices = indices,
    });
}

void RenderSurface::Begin(ArenaAllocator* frame_arena, const GlobalShaderData& global_data) {
    KASSERTM(renderer_data_internal.current_frame_index >= 0, "Did you forget to call Renderer.PrepareFrame()?");
    RendererFrameStats& frame_stats = CurrentFrameStats();
    RendererSurfaceStats& surface_stats = frame_stats.surfaces;
    surface_stats.begin_count++;
    KRAFT_TIMING_STATS_BEGIN(surface_begin_timer);

    this->frame_arena = frame_arena;
    this->max_draw_count = 8192;
    this->draws = ArenaPushArray(this->frame_arena, Draw, this->max_draw_count);
    this->draw_count = 0;
    this->draw_sequence = 0;

    {
        KRAFT_TIMING_STATS_BEGIN(global_ubo_copy_timer);
        MemCpy((void*)ResourceManager->GetBufferData(this->global_ubo), (void*)&global_data, sizeof(global_data));
        KRAFT_TIMING_STATS_ACCUM(global_ubo_copy_timer, surface_stats.global_ubo_copy_ms);
    }

    // Set the active shader variant for this surface
    {
        KRAFT_TIMING_STATS_BEGIN(set_active_variant_timer);
        ShaderSystem::SetActiveVariant(this->variant_name);
        KRAFT_TIMING_STATS_ACCUM(set_active_variant_timer, surface_stats.set_active_variant_ms);
    }

    KRAFT_TIMING_STATS_ACCUM2(surface_begin_timer, frame_stats.total_ms, surface_stats.begin_total_ms);
}

void RenderSurface::Submit(const Renderable& renderable, u16 render_queue) {
    RendererFrameStats& frame_stats = CurrentFrameStats();
    RendererSurfaceStats& surface_stats = frame_stats.surfaces;
    surface_stats.submit_count++;
    KRAFT_TIMING_STATS_BEGIN(surface_submit_timer);

    KASSERTM(this->draw_count < this->max_draw_count, "Draw array overflow! Increase max_draw_count.");

    Handle<Buffer> vertex_buffer = renderable.VertexBuffer ? renderable.VertexBuffer : renderer_data_internal.vertex_buffer;
    Handle<Buffer> index_buffer = renderable.IndexBuffer ? renderable.IndexBuffer : renderer_data_internal.index_buffer;

    DrawConstants* constants = ArenaPush(this->frame_arena, DrawConstants);
    constants->model = renderable.ModelMatrix;
    constants->entity_id = renderable.EntityId;
    constants->material_idx = renderable.MaterialInstance->id;
    constants->mouse_position = this->relative_mouse_position;
    constants->vertex_address = ResourceManager->GetBufferDeviceAddress(vertex_buffer) + renderable.DrawData.VertexByteOffset;

    u16 pipeline_id = renderable.MaterialInstance->shader->ID;
    u64 sort_key = MakeSortKey(render_queue, pipeline_id, renderable.MaterialInstance->id, this->draw_sequence++);
    this->draws[this->draw_count++] = Draw{
        .sort_key = sort_key,
        .index_buffer = index_buffer,
        .draw_constants = constants,
        .custom_bind_group = renderable.CustomBindGroup,
        .index_offset = renderable.DrawData.IndexBufferOffset,
        .index_count = renderable.DrawData.IndexCount,
        .pipeline_id = pipeline_id,
    };

    KRAFT_TIMING_STATS_ACCUM2(surface_submit_timer, frame_stats.total_ms, surface_stats.submit_total_ms);
}

RSFORCEINLINE int draw_less_than(void* element_a, void* element_b) {
    return (*(Draw*)element_a).sort_key < (*(Draw*)element_b).sort_key;
}

void RenderSurface::End() {
    RendererFrameStats& frame_stats = CurrentFrameStats();
    RendererSurfaceStats& surface_stats = frame_stats.surfaces;
    RendererDynamicGeometryStats& dynamic_stats = frame_stats.dynamic_geometry;
    surface_stats.end_count++;
    KRAFT_TIMING_STATS_BEGIN(surface_end_timer);

    if (!this->is_swapchain) {
        KRAFT_TIMING_STATS_BEGIN(begin_surface_timer);
        renderer_data_internal.backend->BeginSurface(this);
        KRAFT_TIMING_STATS_ACCUM(begin_surface_timer, surface_stats.begin_surface_ms);
    }

    u16 last_pipeline_id = UINT16_MAX;
    Handle<BindGroup> last_bind_group;
    Shader* current_shader = nullptr;

#if !KRAFT_DYNAMIC_GEOMETRY_USE_HOST_VISIBLE_BUFFER
    auto dynamic_vertex_buffer_allocator = renderer_data_internal.dynamic_vertex_buffer_allocator[renderer_data_internal.current_frame_index];
    auto dynamic_index_buffer_allocator = renderer_data_internal.dynamic_index_buffer_allocator[renderer_data_internal.current_frame_index];

    // Upload the vertex buffer
    {
        KRAFT_TIMING_STATS_BEGIN(vertex_upload_total_timer);
        auto current = dynamic_vertex_buffer_allocator.head;
        while (current) {
            dynamic_stats.vertex_upload.block_count++;
            dynamic_stats.vertex_upload.bytes_uploaded += dynamic_vertex_buffer_allocator.capacity;
            dynamic_stats.vertex_upload.bytes_used += current->used_bytes;
            if (current->used_bytes > 0) {
                dynamic_stats.vertex_upload.touched_block_count++;
            }

            {
                KRAFT_TIMING_STATS_BEGIN(vertex_upload_timer);
                ResourceManager->UploadBuffer({
                    .DstBuffer = current->gpu_buffer,
                    .SrcBuffer = current->staging_buffer,
                    .SrcSize = dynamic_vertex_buffer_allocator.capacity,
                    .DstOffset = 0,
                    .SrcOffset = 0,
                });
                KRAFT_TIMING_STATS_ACCUM(vertex_upload_timer, dynamic_stats.vertex_upload.total_ms);
            }
            dynamic_stats.vertex_upload.call_count++;

            current = current->next;
        }
        KRAFT_TIMING_STATS_ACCUM(vertex_upload_total_timer, dynamic_stats.upload_total_ms);
    }

    // Upload the index buffer
    {
        KRAFT_TIMING_STATS_BEGIN(index_upload_total_timer);
        auto current = dynamic_index_buffer_allocator.head;
        while (current) {
            dynamic_stats.index_upload.block_count++;
            dynamic_stats.index_upload.bytes_uploaded += dynamic_index_buffer_allocator.capacity;
            dynamic_stats.index_upload.bytes_used += current->used_bytes;
            if (current->used_bytes > 0) {
                dynamic_stats.index_upload.touched_block_count++;
            }

            {
                KRAFT_TIMING_STATS_BEGIN(index_upload_timer);
                ResourceManager->UploadBuffer({
                    .DstBuffer = current->gpu_buffer,
                    .SrcBuffer = current->staging_buffer,
                    .SrcSize = dynamic_index_buffer_allocator.capacity,
                    .DstOffset = 0,
                    .SrcOffset = 0,
                });
                KRAFT_TIMING_STATS_ACCUM(index_upload_timer, dynamic_stats.index_upload.total_ms);
            }
            dynamic_stats.index_upload.call_count++;

            current = current->next;
        }
        KRAFT_TIMING_STATS_ACCUM(index_upload_total_timer, dynamic_stats.upload_total_ms);
    }
#endif

    // Skip drawing if there is nothing to draw
    if (this->draw_count == 0) {
        goto render_surface_end;
    }

    {
        KRAFT_TIMING_STATS_BEGIN(sort_timer);
        radsort(this->draws, this->draw_count, draw_less_than);
        KRAFT_TIMING_STATS_ACCUM(sort_timer, surface_stats.sort_ms);
    }

    // Bind the global shader properties using the first shader
    {
        KRAFT_TIMING_STATS_BEGIN(initial_shader_bind_timer);
        current_shader = ShaderSystem::BindByID(this->draws[0].pipeline_id);
        KRAFT_TIMING_STATS_ACCUM(initial_shader_bind_timer, surface_stats.initial_shader_bind_ms);
    }
    surface_stats.pipeline_bind_count++;
    {
        KRAFT_TIMING_STATS_BEGIN(global_properties_timer);
        renderer_data_internal.backend->ApplyGlobalShaderProperties(current_shader, this->global_ubo, renderer_data_internal.materials_gpu_buffer);
        KRAFT_TIMING_STATS_ACCUM(global_properties_timer, surface_stats.global_properties_ms);
    }
    surface_stats.global_properties_count++;

    for (u32 i = 0; i < this->draw_count; i++) {
        Draw& draw = this->draws[i];

        if (draw.pipeline_id != last_pipeline_id) {
            {
                KRAFT_TIMING_STATS_BEGIN(pipeline_switch_timer);
                current_shader = ShaderSystem::BindByID(draw.pipeline_id);
                KRAFT_TIMING_STATS_ACCUM(pipeline_switch_timer, surface_stats.pipeline_switch_ms);
            }
            surface_stats.pipeline_bind_count++;
            // renderer_data_internal.backend->ApplyGlobalShaderProperties(current_shader, this->global_ubo, renderer_data_internal.materials_gpu_buffer);
            last_pipeline_id = draw.pipeline_id;
            last_bind_group = Handle<BindGroup>();
        }

        if (draw.custom_bind_group && draw.custom_bind_group != last_bind_group) {
            {
                KRAFT_TIMING_STATS_BEGIN(custom_bind_group_timer);
                renderer_data_internal.backend->BindBindGroup(current_shader, draw.custom_bind_group);
                KRAFT_TIMING_STATS_ACCUM(custom_bind_group_timer, surface_stats.custom_bind_group_ms);
            }
            surface_stats.custom_bind_group_bind_count++;
            last_bind_group = draw.custom_bind_group;
        }

        {
            KRAFT_TIMING_STATS_BEGIN(local_properties_timer);
            renderer_data_internal.backend->ApplyLocalShaderProperties(current_shader, draw.draw_constants);
            KRAFT_TIMING_STATS_ACCUM(local_properties_timer, surface_stats.local_properties_ms);
        }
        surface_stats.local_properties_count++;

        {
            KRAFT_TIMING_STATS_BEGIN(draw_geometry_timer);
            renderer_data_internal.backend->DrawGeometryData(
                {
                    .IndexCount = draw.index_count,
                    .IndexBufferOffset = draw.index_offset,
                },
                draw.index_buffer
            );
            KRAFT_TIMING_STATS_ACCUM(draw_geometry_timer, surface_stats.draw_geometry_ms);
        }
        surface_stats.draw_call_count++;
    }

render_surface_end:
    if (!this->is_swapchain) {
        KRAFT_TIMING_STATS_BEGIN(end_surface_timer);
        renderer_data_internal.backend->EndSurface(this);
        KRAFT_TIMING_STATS_ACCUM(end_surface_timer, surface_stats.end_surface_ms);
    }

    // Reset active variant
    {
        KRAFT_TIMING_STATS_BEGIN(reset_active_variant_timer);
        ShaderSystem::SetActiveVariant({});
        KRAFT_TIMING_STATS_ACCUM(reset_active_variant_timer, surface_stats.reset_active_variant_ms);
    }

    KRAFT_TIMING_STATS_ACCUM2(surface_end_timer, frame_stats.total_ms, surface_stats.end_total_ms);
}

RendererFrontend* CreateRendererFrontend(const RendererOptions* Opts) {
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

    if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN) {
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
        renderer_data_internal.backend->UpdateGeometry = VulkanRendererBackend::UpdateGeometry;
        renderer_data_internal.backend->DrawGeometryData = VulkanRendererBackend::DrawGeometryData;

        // Render Passes
        renderer_data_internal.backend->BeginSurface = VulkanRendererBackend::BeginSurface;
        renderer_data_internal.backend->EndSurface = VulkanRendererBackend::EndSurface;

        // Commands
        renderer_data_internal.backend->CmdSetCustomBuffer = VulkanRendererBackend::CmdSetCustomBuffer;
        renderer_data_internal.backend->BindBindGroup = VulkanRendererBackend::BindBindGroup;
    } else if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL) {
        return nullptr;
    }

    if (!renderer_data_internal.backend->Init(renderer_data_internal.arena, g_Renderer->Settings)) {
        KERROR("[RendererFrontend::Init]: Failed to initialize renderer backend!");
        return nullptr;
    }

    // Post-init
    if (g_Renderer->Settings->Backend == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN) {
        VulkanRendererBackend::SetDeviceData(g_Device);
    }

    g_Renderer->Init();

    return g_Renderer;
}

void DestroyRendererFrontend(RendererFrontend* Instance) {
    renderer_data_internal.backend->Shutdown();
    MemZero(renderer_data_internal.backend, sizeof(RendererBackend));

    DestroyVulkanResourceManager(ResourceManager);
    DestroyArena(renderer_data_internal.arena);
}

SpriteBatch* CreateSpriteBatch(ArenaAllocator* arena, u16 batch_size) {
    u32 vertex_count = batch_size * 4;
    u32 index_count = batch_size * 6;
    SpriteBatch* batch = ArenaPush(arena, SpriteBatch);
    batch->vertices = ArenaPushArray(arena, Vertex2D, vertex_count);
    batch->indices = ArenaPushArray(arena, u32, index_count);

    // We will write the indices once
    for (u16 i = 0; i < batch_size; i++) {
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

void BeginSpriteBatch(SpriteBatch* batch, Material* material) {
    batch->material = material;
    batch->quad_count = 0;
}

bool DrawQuad(SpriteBatch* batch, vec2 position, vec2 size, vec2 uv_min, vec2 uv_max, vec4 color) {
    if (batch->quad_count == batch->max_quad_count) {
        KWARN("[SpriteBatch::DrawQuad]: Batch is already full");
        return false;
    }

    batch->vertices[batch->quad_count * 4 + 0] = Vertex2D{
        .Position = {position.x + size.x, position.y + size.y},
        .UV = {uv_max.x, uv_max.y},
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 1] = Vertex2D{
        .Position = {position.x + size.x, position.y},
        .UV = {uv_max.x, uv_min.y},
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 2] = Vertex2D{
        .Position = {position.x, position.y},
        .UV = {uv_min.x, uv_min.y},
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 3] = Vertex2D{
        .Position = {position.x, position.y + size.y},
        .UV = {uv_min.x, uv_max.y},
        .Color = color,
    };

    batch->quad_count += 1;

    return true;
}

bool DrawQuad(SpriteBatch* batch, vec2 p0, vec2 p1, vec2 p2, vec2 p3, vec2 uv_min, vec2 uv_max, vec4 color) {
    if (batch->quad_count == batch->max_quad_count) {
        KWARN("[SpriteBatch::DrawQuad]: Batch is already full");
        return false;
    }

    batch->vertices[batch->quad_count * 4 + 0] = Vertex2D{
        .Position = p0,
        .UV = {uv_max.x, uv_max.y},
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 1] = Vertex2D{
        .Position = p1,
        .UV = {uv_max.x, uv_min.y},
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 2] = Vertex2D{
        .Position = p2,
        .UV = {uv_min.x, uv_min.y},
        .Color = color,
    };

    batch->vertices[batch->quad_count * 4 + 3] = Vertex2D{
        .Position = p3,
        .UV = {uv_min.x, uv_max.y},
        .Color = color,
    };

    batch->quad_count += 1;

    return true;
}

void EndSpriteBatch(SpriteBatch* batch) {
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
