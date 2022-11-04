#include "simple_scene.h"

#include "renderer/kraft_renderer_types.h"
#include "renderer/vulkan/kraft_vulkan_shader.h"
#include "renderer/vulkan/kraft_vulkan_buffer.h"
#include "renderer/vulkan/kraft_vulkan_vertex_buffer.h"
#include "renderer/vulkan/kraft_vulkan_index_buffer.h"
#include "renderer/vulkan/kraft_vulkan_pipeline.h"

#include "renderer/kraft_renderer_frontend.h"

#include <imgui.h>

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
};

static VulkanBuffer CameraUniformBuffer = {};
static VulkanBuffer VertexBuffer;
static VulkanBuffer IndexBuffer;
static uint32 IndexCount = 0;
static VkDescriptorSet GlobalDescriptorSets[3] = {};
static SimpleObjectState ObjectState;
static VkDescriptorPool DescriptorPool;
static VkDescriptorSetLayout DescriptorSetLayout;

static void ImGuiWidgets()
{
    ImGui::Begin("Debug");

    static float    left   = -1024.0f * 0.5f, 
                    right  = 1024.0f * 0.5f, 
                    top    = -768.0f * 0.5f, 
                    bottom = 768.0f * 0.5f, 
                    nearClip = -1.f, 
                    farClip = 1.f;

    static float fov = 45.f;
    static float width = 1024.f;
    static float height = 768.f;
    static float nearClipP = 0.1f;
    static float farClipP = 1000.f;

    static bool usePerspectiveProjection = true;

    if (ImGui::RadioButton("Perspective Projection", usePerspectiveProjection == true))
    {
        usePerspectiveProjection = true;
        ObjectState.CameraData.Projection = PerspectiveMatrix(DegreeToRadians(45.0f), 1024.f / 768.f, 0.1f, 1000.f);
        ObjectState.CameraData.View = TranslationMatrix(Vec3f(0.0f, 0.0f, -30.f));
    }

    if (ImGui::RadioButton("Orthographic Projection", usePerspectiveProjection == false))
    {
        usePerspectiveProjection = false;
        ObjectState.CameraData.Projection = OrthographicMatrix(-1024.0f * 0.5f, 1024.0f * 0.5f, -768.0f * 0.5f, 768.0f * 0.5f, -1.0f, 1.0f);
        ObjectState.CameraData.View = TranslationMatrix(Vec3f(0.0f, 0.0f, 0.0f));
    }

    if (usePerspectiveProjection)
    {
        ImGui::Text("Perspective Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("FOV", &fov))
        {
            ObjectState.CameraData.Projection = PerspectiveMatrix(DegreeToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Width", &width))
        {
            ObjectState.CameraData.Projection = PerspectiveMatrix(DegreeToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Height", &height))
        {
            ObjectState.CameraData.Projection = PerspectiveMatrix(DegreeToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Near clip", &nearClipP))
        {
            ObjectState.CameraData.Projection = PerspectiveMatrix(DegreeToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Far clip", &farClipP))
        {
            ObjectState.CameraData.Projection = PerspectiveMatrix(DegreeToRadians(fov), width / height, nearClipP, farClipP);
        }
    }
    else
    {
        ImGui::Text("Orthographic Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("Left", &left))
        {
            ObjectState.CameraData.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Right", &right))
        {
            ObjectState.CameraData.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Top", &top))
        {
            ObjectState.CameraData.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Bottom", &bottom))
        {
            ObjectState.CameraData.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Near", &nearClip))
        {
            ObjectState.CameraData.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Far", &farClip))
        {
            ObjectState.CameraData.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }
    }

    ImGui::Separator();
    ImGui::Text("Tranform");
    ImGui::Separator();
    ImGui::DragFloat4("Translation", ObjectState.CameraData.View[3]._data);
    
    ImGui::End();
}

void InitTestScene(VulkanContext* context)
{
    // ObjectState.CameraData.Projection = OrthographicMatrix(-1024.0f * 0.5f, 1024.0f * 0.5f, -768.0f * 0.5f, 768.0f * 0.5f, -1.0f, 1.0f);
    ObjectState.CameraData.Projection = PerspectiveMatrix(DegreeToRadians(45.0f), 1024.f / 768.f, 0.1f, 1000.f);
    ObjectState.CameraData.View = TranslationMatrix(Vec3f(0.0f, 0.0f, -30.f));
    ObjectState.ModelMatrix = Mat4f(Identity);

    RendererFrontend::I->ImGuiRenderer.AddWidget("demo window", ImGuiWidgets);

    VkShaderModule vertex, fragment;
    VulkanCreateShaderModule(context, "res/shaders/vertex.vert.spv", &vertex);
    assert(vertex);

    VulkanCreateShaderModule(context, "res/shaders/fragment.frag.spv", &fragment);
    assert(fragment);

    VkPipelineShaderStageCreateInfo vertexShaderStage = {};
    vertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStage.module = vertex;
    vertexShaderStage.pName = "main";
    vertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineShaderStageCreateInfo fragmentShaderStage = {};
    fragmentShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStage.module = fragment;
    fragmentShaderStage.pName = "main";
    fragmentShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineShaderStageCreateInfo stages[] = {
        vertexShaderStage,
        fragmentShaderStage
    };

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = (float32)context->FramebufferHeight;
    viewport.width = (float32)context->FramebufferWidth;
    viewport.height = -(float32)context->FramebufferHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent = {context->FramebufferWidth, context->FramebufferHeight};
    
    VkVertexInputAttributeDescription inputAttributeDesc[2];
    inputAttributeDesc[0].location = 0;
    inputAttributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttributeDesc[0].binding = 0;
    inputAttributeDesc[0].offset = offsetof(Vertex3D, Position);

    inputAttributeDesc[1].location = 1;
    inputAttributeDesc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    inputAttributeDesc[1].binding = 0;
    inputAttributeDesc[1].offset = offsetof(Vertex3D, Color);

    // Descriptor Sets
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
    descriptorSetLayoutBinding.binding = 0;
    descriptorSetLayoutBinding.descriptorCount = 1;
    descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetLayoutBinding.pImmutableSamplers = 0;
    descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

    KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(context->LogicalDevice.Handle, &descriptorSetLayoutCreateInfo, context->AllocationCallbacks, &DescriptorSetLayout));

    VkDescriptorPoolSize descriptorPoolSize = {};
    descriptorPoolSize.descriptorCount = context->Swapchain.ImageCount;
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
    descriptorPoolCreateInfo.maxSets = context->Swapchain.ImageCount;

    vkCreateDescriptorPool(context->LogicalDevice.Handle, &descriptorPoolCreateInfo, context->AllocationCallbacks, &DescriptorPool);

    VulkanPipelineDescription pipelineDesc = {};
    pipelineDesc.IsWireframe = false;
    pipelineDesc.ShaderStageCount = sizeof(stages) / sizeof(stages[0]);
    pipelineDesc.ShaderStages = stages;
    pipelineDesc.Viewport = viewport;
    pipelineDesc.Scissor = scissor;
    pipelineDesc.Attributes = inputAttributeDesc;
    pipelineDesc.AttributeCount = sizeof(inputAttributeDesc) / sizeof(inputAttributeDesc[0]);
    pipelineDesc.DescriptorSetLayoutCount = 1;
    pipelineDesc.DescriptorSetLayouts = &DescriptorSetLayout;

    ObjectState.Pipeline = {};
    VulkanCreateGraphicsPipeline(context, &context->MainRenderPass, pipelineDesc, &ObjectState.Pipeline);
    assert(ObjectState.Pipeline.Handle);

    VulkanDestroyShaderModule(context, &vertex);
    VulkanDestroyShaderModule(context, &fragment);

    // Create a buffer where we can push camera data
    VulkanCreateBuffer(context, sizeof(CameraBuffer) * 3, VK_SHARING_MODE_EXCLUSIVE, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true, &CameraUniformBuffer);

    assert(CameraUniformBuffer.Handle);

    VkDescriptorSetLayout descriptorSets[3] = {
        DescriptorSetLayout,
        DescriptorSetLayout,
        DescriptorSetLayout,
    };

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    descriptorSetAllocateInfo.descriptorPool = DescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = sizeof(descriptorSets) / sizeof(descriptorSets[0]);
    descriptorSetAllocateInfo.pSetLayouts = descriptorSets;

    KRAFT_VK_CHECK(vkAllocateDescriptorSets(context->LogicalDevice.Handle, &descriptorSetAllocateInfo, GlobalDescriptorSets));
    
    // #5865f2
    // Vec3f color = Vec3f(0x58 / 255.0f, 0x65 / 255.0f, 0xf2 / 255.0f);
    Vec4f color = KRGBA(0x58, 0x65, 0xf2, 0x1f);
    Vec4f colorB = KRGBA(0xff, 0x6b, 0x6b, 0xff);
    // Vertex and index buffers
    Vertex3D verts[] = {
        {Vec3f(+0.5f, +0.5f, +0.0f), colorB},
        {Vec3f(-0.5f, -0.5f, +0.0f), color},
        {Vec3f(+0.5f, -0.5f, +0.0f), color},
        {Vec3f(-0.5f, +0.5f, +0.0f), colorB},
    };

    const float32 scale = 10.f;
    const int vertsCount = sizeof(verts) / sizeof(verts[0]);
    for (int i = 0; i < vertsCount; ++i)
    {
        for (int j = 0; j < 2; j++)
        {
            verts[i].Position._data[j] *= scale;
        }
    }

    uint32 indices[] = {0, 1, 2, 3, 1, 0};
    IndexCount = sizeof(indices) / sizeof(indices[0]);
    VulkanCreateVertexBuffer(context, verts, sizeof(verts), &VertexBuffer);
    VulkanCreateIndexBuffer(context, indices, sizeof(indices), &IndexBuffer);
}

void RenderTestScene(VulkanContext* context, VulkanCommandBuffer* buffer)
{
    VulkanBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, &ObjectState.Pipeline);
    
    vkCmdPushConstants(buffer->Handle, ObjectState.Pipeline.Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ObjectState.ModelMatrix), &ObjectState.ModelMatrix);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(buffer->Handle, 0, 1, &VertexBuffer.Handle, offsets);
    vkCmdBindIndexBuffer(buffer->Handle, IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);

    // static float32 z = -1.0f;
    // z -= 0.005f;
    // ObjectState.CameraData.View = TranslationMatrix(Vec3f(0.0f, 0.0f, z));
    VulkanLoadDataInBuffer(context, &CameraUniformBuffer, &ObjectState.CameraData, sizeof(CameraBuffer), 0);

    VkDescriptorBufferInfo descriptorBufferInfo;
    descriptorBufferInfo.buffer = CameraUniformBuffer.Handle;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = sizeof(CameraBuffer);

    VkWriteDescriptorSet descriptorWriteInfo = {};
    descriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWriteInfo.descriptorCount = 1;
    descriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWriteInfo.dstSet = GlobalDescriptorSets[context->CurrentSwapchainImageIndex];
    descriptorWriteInfo.dstBinding = 0;
    descriptorWriteInfo.dstArrayElement = 0;
    descriptorWriteInfo.pBufferInfo = &descriptorBufferInfo;

    vkUpdateDescriptorSets(context->LogicalDevice.Handle, 1, &descriptorWriteInfo, 0, 0);

    vkCmdBindDescriptorSets(buffer->Handle, VK_PIPELINE_BIND_POINT_GRAPHICS, ObjectState.Pipeline.Layout, 0, 1, &GlobalDescriptorSets[context->CurrentSwapchainImageIndex], 0, 0);

    // vkCmdDraw(buffer->Handle, 3, 1, 0, 0);
    vkCmdDrawIndexed(buffer->Handle, IndexCount, 1, 0, 0, 0);
}

void DestroyTestScene(VulkanContext* context)
{
    VulkanDestroyBuffer(context, &CameraUniformBuffer);
    VulkanDestroyBuffer(context, &VertexBuffer);
    VulkanDestroyBuffer(context, &IndexBuffer);

    VulkanDestroyPipeline(context, &ObjectState.Pipeline);
    ObjectState.Pipeline = {};

    vkDestroyDescriptorPool(context->LogicalDevice.Handle, DescriptorPool, context->AllocationCallbacks);
    vkDestroyDescriptorSetLayout(context->LogicalDevice.Handle, DescriptorSetLayout, context->AllocationCallbacks);
}


}