#include "simple_scene.h"

#include "core/kraft_imgui.h"

#include "systems/kraft_material_system.h"
#include "systems/kraft_shader_system.h"

#define MAX_OBJECT_COUNT 128

namespace kraft::renderer
{

static SceneState TestSceneState = {};

SceneState* GetSceneState()
{
    return &TestSceneState;
}

void RenderTestScene(VulkanContext* context, VulkanCommandBuffer* buffer)
{
    ShaderSystem::Bind(TestSceneState.ObjectState.Material->Shader);

    MaterialSystem::ApplyGlobalProperties(TestSceneState.ObjectState.Material, TestSceneState.ProjectionMatrix, TestSceneState.ViewMatrix);
    MaterialSystem::ApplyInstanceProperties(TestSceneState.ObjectState.Material);
    MaterialSystem::ApplyLocalProperties(TestSceneState.ObjectState.Material, TestSceneState.ObjectState.ModelMatrix);

    VkDeviceSize offsets[1] = {context->Geometries[TestSceneState.ObjectState.Geometry->InternalID].VertexBufferOffset};
    vkCmdBindVertexBuffers(buffer->Handle, 0, 1, &context->VertexBuffer.Handle, offsets);
    vkCmdBindIndexBuffer(buffer->Handle, context->IndexBuffer.Handle, context->Geometries[TestSceneState.ObjectState.Geometry->InternalID].IndexBufferOffset, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(buffer->Handle, context->Geometries[TestSceneState.ObjectState.Geometry->InternalID].IndexCount, 1, 0, 0, 0);
    ShaderSystem::Unbind();
}

}