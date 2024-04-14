#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "renderer/kraft_camera.h"
#include "resources/kraft_resource_types.h"

namespace kraft::renderer
{

struct SimpleObjectState
{
    Mat4f                    ModelMatrix;
    Vec3f                    Scale = Vec3fZero;
    Vec3f                    Position = Vec3fZero;
    Vec3f                    Rotation = Vec3fZero;

    Material*                Material = nullptr;
    Geometry*                Geometry = nullptr;
    bool                     Dirty;
};

struct SceneState
{
    Mat4f               ProjectionMatrix;
    Mat4f               ViewMatrix;
    Camera              SceneCamera;
    SimpleObjectState   ObjectState;

    enum ProjectionType
    {
        Perspective,
        Orthographic
    } Projection;
};

void RenderTestScene(VulkanContext* context, VulkanCommandBuffer* buffer);
SceneState* GetSceneState();

}