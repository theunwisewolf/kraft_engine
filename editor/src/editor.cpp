#include "editor.h"

#include "kraft_includes.h"

EditorState* EditorState::Ptr = nullptr;

using namespace kraft;
using namespace kraft::r;

EditorState::EditorState()
{
    EditorState::Ptr = this;
    this->RenderSurface = kraft::g_Renderer->CreateRenderSurface(S("SceneView"), 200.0f, 200.0f, true);
    this->ObjectPickingRenderSurface = kraft::g_Renderer->CreateRenderSurface(S("ObjectPickingView"), 200.0f, 200.0f, true);

    this->RenderSurface.relative_mouse_position = { 999999.0f, 999999.0f };
    this->ObjectPickingRenderSurface.relative_mouse_position = { 999999.0f, 999999.0f };

    u64       ExtraMemoryFlags = g_Device->supports_device_local_host_visible ? MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL : 0;
    const u32 picking_buffer_size = 64;
    this->picking_buffer = kraft::r::ResourceManager->CreateBuffer({
        .DebugName = "EditorPickingDataBuffer",
        .Size = (u64)math::AlignUp(picking_buffer_size * sizeof(u32), kraft::g_Device->min_storage_buffer_alignment),
        .UsageFlags = BufferUsageFlags::BUFFER_USAGE_FLAGS_STORAGE_BUFFER,
        .MemoryPropertyFlags = MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_VISIBLE | MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_HOST_COHERENT | ExtraMemoryFlags,
        .MapMemory = true,
    });

    // Map the buffer memory
    this->picking_buffer_memory = ResourceManager->GetBufferData(this->picking_buffer);
}

kraft::Entity EditorState::GetSelectedEntity()
{
    return this->CurrentWorld->GetEntity(SelectedEntity);
}

void EditorState::SetCamera(const kraft::Camera& Camera)
{
    this->CurrentWorld->Camera = Camera;
}
