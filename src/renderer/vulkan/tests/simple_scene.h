#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "renderer/kraft_camera.h"
#include "resources/kraft_resource_types.h"

namespace kraft
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
    VulkanPipeline          Pipeline;
    ObjectUniformBuffer     UBO;
    Mat4f                   ModelMatrix;

    VkDescriptorSet          DescriptorSets[3]; // Per frame
    VulkanDescriptorSetState DescriptorSetStates[KRAFT_VULKAN_SHADER_MAX_BINDINGS];
    Texture*                 Texture;

    void AcquireResources(VulkanContext *context);
    void ReleaseResources(VulkanContext *context);
};

struct SceneState
{
    GlobalUniformBuffer      GlobalUBO;
    VkDescriptorSetLayout    GlobalDescriptorSetLayout;
    VkDescriptorSetLayout    LocalDescriptorSetLayout;
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
void DestroyTestScene(VulkanContext* context);

SceneState* GetSceneState();

}