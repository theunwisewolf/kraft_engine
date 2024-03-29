#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "renderer/kraft_camera.h"
#include "resources/kraft_resource_types.h"

namespace kraft::renderer
{

struct GlobalUniformBuffer
{
    union
    {
        struct
        {
            Mat4f Projection;
            Mat4f View;
        };

        char _[256];
    };

    GlobalUniformBuffer() {}
};

struct ObjectUniformBuffer
{
    union
    {
        struct
        {
            Vec4f DiffuseColor;
        };
    };

    ObjectUniformBuffer() {}
};

struct SimpleObjectState
{
    RenderPipeline          Pipeline;
    ObjectUniformBuffer     UBO;
    Mat4f                   ModelMatrix;
    Vec3f                   Scale = Vec3fZero;
    Vec3f                   Position = Vec3fZero;
    Vec3f                   Rotation = Vec3fZero;

    VkDescriptorSet          DescriptorSets[3]; // Per frame
    VulkanDescriptorSetState DescriptorSetStates[KRAFT_VULKAN_SHADER_MAX_BINDINGS];
    Material*                Material = NULL;
    bool                     Dirty;

    void AcquireResources(VulkanContext *context);
    void ReleaseResources(VulkanContext *context);
};

struct SceneState
{
    GlobalUniformBuffer      GlobalUBO;
    VkDescriptorSetLayout    GlobalDescriptorSetLayout;
    VulkanBuffer             GlobalUniformBuffer;
    VulkanBuffer             LocalUniformBuffer;
    VkDescriptorPool         GlobalDescriptorPool;
    VkDescriptorPool         LocalDescriptorPool;
    VkDescriptorSet          GlobalDescriptorSets[3]; // Per frame
    Camera                   SceneCamera;

    enum ProjectionType
    {
        Perspective,
        Orthographic
    } Projection;
};

void InitTestScene(VulkanContext* context);
void RenderTestScene(VulkanContext* context, VulkanCommandBuffer* buffer);
void UpdateDescriptorSets(VulkanContext *context);
void DestroyTestScene(VulkanContext* context);
void SetProjection(SceneState::ProjectionType projection);

SceneState* GetSceneState();

}