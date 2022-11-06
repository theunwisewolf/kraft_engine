#pragma once

#include "core/kraft_core.h"
#include "renderer/vulkan/kraft_vulkan_types.h"
#include "renderer/kraft_camera.h"

namespace kraft
{

struct CameraBuffer
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

    CameraBuffer() {}
};

struct SimpleObjectState
{
    VulkanPipeline  Pipeline;
    CameraBuffer    CameraData;
    Mat4f           ModelMatrix;
    Camera          SceneCamera;
};

void InitTestScene(VulkanContext* context);
void RenderTestScene(VulkanContext* context, VulkanCommandBuffer* buffer);
void DestroyTestScene(VulkanContext* context);

SimpleObjectState* GetObjectState();

}