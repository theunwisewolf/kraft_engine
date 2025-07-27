#include "editor.h"

#include "world/kraft_entity.h"
#include <core/kraft_engine.h>
#include <math/kraft_math.h>
#include <renderer/kraft_renderer_frontend.h>
#include <renderer/kraft_resource_manager.h>
#include <systems/kraft_shader_system.h>

#include <renderer/kraft_renderer_types.h>

EditorState* EditorState::Ptr = nullptr;

using namespace kraft;
using namespace kraft::renderer;

EditorState::EditorState()
{
    EditorState::Ptr = this;
    this->RenderSurface = kraft::g_Renderer->CreateRenderSurface("SceneView", 200.0f, 200.0f, true);
    this->ObjectPickingRenderSurface = kraft::g_Renderer->CreateRenderSurface("ObjectPickingView", 200.0f, 200.0f, true);

    this->RenderSurface.RelativeMousePosition = { 999999.0f, 999999.0f };
    this->ObjectPickingRenderSurface.RelativeMousePosition = { 999999.0f, 999999.0f };

    uint64    ExtraMemoryFlags = g_Device->supports_device_local_host_visible ? MemoryPropertyFlags::MEMORY_PROPERTY_FLAGS_DEVICE_LOCAL : 0;
    const uint32 picking_buffer_size = 64;
    this->picking_buffer = kraft::renderer::ResourceManager->CreateBuffer({
        .DebugName = "EditorPickingDataBuffer",
        .Size = (uint64)math::AlignUp(picking_buffer_size * sizeof(uint32), kraft::g_Device->min_storage_buffer_alignment),
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
