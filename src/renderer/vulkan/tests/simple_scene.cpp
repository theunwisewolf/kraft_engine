#include "simple_scene.h"

#include "core/kraft_events.h"
#include "core/kraft_input.h"

#include "renderer/kraft_renderer_types.h"
#include "renderer/vulkan/kraft_vulkan_shader.h"
#include "renderer/vulkan/kraft_vulkan_buffer.h"
#include "renderer/vulkan/kraft_vulkan_vertex_buffer.h"
#include "renderer/vulkan/kraft_vulkan_index_buffer.h"
#include "renderer/vulkan/kraft_vulkan_pipeline.h"
#include "renderer/vulkan/kraft_vulkan_texture.h"

#include "renderer/kraft_renderer_frontend.h"
#include "systems/kraft_texture_system.h"

#include <imgui.h>

#include "stb_image.h"

#define MAX_OBJECT_COUNT 128

namespace kraft
{

static VulkanBuffer VertexBuffer;
static VulkanBuffer IndexBuffer;
static uint32 IndexCount = 0;
static SimpleObjectState ObjectState;

static SceneState TestSceneState = {};
static char TextureName[] = "res/textures/texture.jpg";

bool KeyDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    static bool defaultTexture = false;
    kraft::Keys keycode = (kraft::Keys)data.Int32[0];
    if (keycode == kraft::KEY_C)
    {
        ObjectState.Texture = defaultTexture ? TextureSystem::AcquireTexture(TextureName) : TextureSystem::GetDefaultDiffuseTexture();
        defaultTexture = !defaultTexture;
        for (int i = 0; i < KRAFT_VULKAN_SHADER_MAX_BINDINGS; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                ObjectState.DescriptorSetStates[i].Generations[j] = KRAFT_INVALID_ID_UINT8;
            }
        }
    }

    KINFO("%s key pressed (code = %d)", kraft::Platform::GetKeyName(keycode), keycode);

    return false;
}

SceneState* GetSceneState()
{
    return &TestSceneState;
}

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

    // Model
    static Vec3f scale = {10.0f, 10.0f, 10.0f};
    static Vec3f position = Vec3fZero;
    static Vec3f rotationDeg = Vec3fZero;
    static Vec3f rotation = Vec3fZero;

    static bool usePerspectiveProjection = true;

    kraft::Camera& camera = TestSceneState.SceneCamera;
    if (ImGui::RadioButton("Perspective Projection", usePerspectiveProjection == true))
    {
        usePerspectiveProjection = true;
        TestSceneState.Projection = SceneState::ProjectionType::Perspective;
        TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(45.0f), 1024.f / 768.f, 0.1f, 1000.f);

        camera.Reset();
        camera.SetPosition({0.0f, 0.0f, 30.f});

        scale = {10.0f, 10.0f, 10.0f};
    }

    if (ImGui::RadioButton("Orthographic Projection", usePerspectiveProjection == false))
    {
        usePerspectiveProjection = false;
        TestSceneState.Projection = SceneState::ProjectionType::Orthographic;
        TestSceneState.GlobalUBO.Projection = OrthographicMatrix(-1024.0f * 0.5f, 1024.0f * 0.5f, -768.0f * 0.5f, 768.0f * 0.5f, -1.0f, 1.0f);

        camera.Reset();
        camera.SetPosition(Vec3fZero);

        scale = {512.0f, 512.0f, 512.0f};
    }

    if (usePerspectiveProjection)
    {
        ImGui::Text("Perspective Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("FOV", &fov))
        {
            TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Width", &width))
        {
            TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Height", &height))
        {
            TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Near clip", &nearClipP))
        {
            TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(fov), width / height, nearClipP, farClipP);
        }

        if (ImGui::DragFloat("Far clip", &farClipP))
        {
            TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(fov), width / height, nearClipP, farClipP);
        }
    }
    else
    {
        ImGui::Text("Orthographic Projection Matrix");
        ImGui::Separator();

        if (ImGui::DragFloat("Left", &left))
        {
            TestSceneState.GlobalUBO.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Right", &right))
        {
            TestSceneState.GlobalUBO.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Top", &top))
        {
            TestSceneState.GlobalUBO.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Bottom", &bottom))
        {
            TestSceneState.GlobalUBO.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Near", &nearClip))
        {
            TestSceneState.GlobalUBO.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }

        if (ImGui::DragFloat("Far", &farClip))
        {
            TestSceneState.GlobalUBO.Projection = OrthographicMatrix(left, right, top, bottom, nearClip, farClip);
        }
    }

    ImGui::PushID("CAMERA_TRANSFORM");
    {
        ImGui::Separator();
        ImGui::Text("Camera Transform");
        ImGui::Separator();
        ImGui::DragFloat4("Translation", camera.GetViewMatrix()[3]._data);
    }
    ImGui::PopID();

    ImGui::PushID("OBJECT_TRANSFORM");
    {
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Separator();
        
        if (ImGui::DragFloat3("Scale", scale._data))
        {
            KINFO("Scale - %f, %f, %f", scale.x, scale.y, scale.z);
            // ObjectState.ModelMatrix = ScaleMatrix(Vec3f{scale.x, scale.y, scale.z});
        }

        if (ImGui::DragFloat3("Translation", position._data))
        {
            KINFO("Translation - %f, %f, %f", position.x, position.y, position.z);
            // ObjectState.ModelMatrix *= TranslationMatrix(position);
        }

        if (ImGui::DragFloat3("Rotation Degrees", rotationDeg._data))
        {
            rotation.x = DegToRadians(rotationDeg.x);
            rotation.y = DegToRadians(rotationDeg.y);
            rotation.z = DegToRadians(rotationDeg.z);
        }
    }
    ImGui::PopID();

    ObjectState.ModelMatrix = ScaleMatrix(scale) * RotationMatrixFromEulerAngles(rotation) * TranslationMatrix(position);
    ImGui::End();
}

void SimpleObjectState::AcquireResources(VulkanContext *context)
{
    VkDescriptorSetLayout descriptorSets[] = {
        TestSceneState.LocalDescriptorSetLayout,
        TestSceneState.LocalDescriptorSetLayout,
        TestSceneState.LocalDescriptorSetLayout,
    };

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    descriptorSetAllocateInfo.descriptorPool = TestSceneState.LocalDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = sizeof(descriptorSets) / sizeof(descriptorSets[0]);
    descriptorSetAllocateInfo.pSetLayouts = descriptorSets;

    KRAFT_VK_CHECK(vkAllocateDescriptorSets(context->LogicalDevice.Handle, &descriptorSetAllocateInfo, ObjectState.DescriptorSets));

    for (int i = 0; i < KRAFT_VULKAN_SHADER_MAX_BINDINGS; ++i)
    {
        for (int j = 0; j < 3; j++)
            ObjectState.DescriptorSetStates[i].Generations[j] = KRAFT_INVALID_ID_UINT8;
    }
}

void SimpleObjectState::ReleaseResources(VulkanContext *context)
{
    TextureSystem::ReleaseTexture(TextureName);
    // KRAFT_VK_CHECK(vkFreeDescriptorSets(context->LogicalDevice.Handle, TestSceneState.LocalDescriptorPool, 3, ObjectState.DescriptorSets));

    for (int i = 0; i < KRAFT_VULKAN_SHADER_MAX_BINDINGS; ++i)
    {
        for (int j = 0; j < 3; j++)
            ObjectState.DescriptorSetStates[i].Generations[j] = -1;
    }
}

void InitTestScene(VulkanContext* context)
{
    EventSystem::Listen(EVENT_TYPE_KEY_DOWN, &TestSceneState, KeyDownEventListener);
    TestSceneState.Projection = SceneState::ProjectionType::Perspective;

    // Load textures
    ObjectState.Texture = TextureSystem::AcquireTexture(TextureName);
    // ObjectState.Texture = TextureSystem::GetDefaultDiffuseTexture();

    TestSceneState.SceneCamera.SetPosition(Vec3f(0.0f, 0.0f, 30.f));
    ObjectState.ModelMatrix = ScaleMatrix(Vec3f(10.f, 10.f, 1.f));
    ObjectState.UBO.DiffuseColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(45.0f), 1024.f / 768.f, 0.1f, 1000.f);
    TestSceneState.GlobalUBO.View = TestSceneState.SceneCamera.GetViewMatrix();
    TestSceneState.SceneCamera.Dirty = true;

    // VulkanCreateDefaultTexture(context, 256, 256, 4, &ObjectState.Texture);

    Renderer->ImGuiRenderer.AddWidget("demo window", ImGuiWidgets);

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
    inputAttributeDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
    inputAttributeDesc[1].binding = 0;
    inputAttributeDesc[1].offset = offsetof(Vertex3D, UV);

    // Global Descriptor Sets
    {
        VkDescriptorSetLayoutBinding globalDescriptorSetLayoutBinding = {};
        globalDescriptorSetLayoutBinding.binding = 0;
        globalDescriptorSetLayoutBinding.descriptorCount = 1;
        globalDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        globalDescriptorSetLayoutBinding.pImmutableSamplers = 0;
        globalDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo globalDescriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        globalDescriptorSetLayoutCreateInfo.bindingCount = 1;
        globalDescriptorSetLayoutCreateInfo.pBindings = &globalDescriptorSetLayoutBinding;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(context->LogicalDevice.Handle, &globalDescriptorSetLayoutCreateInfo, context->AllocationCallbacks, &TestSceneState.GlobalDescriptorSetLayout));
    }

    // Local Descriptor Sets
    {
        VkDescriptorSetLayoutBinding objectDescriptorSetLayoutBinding[2] = {};
        
        // Material data
        // Currently only diffuse color
        objectDescriptorSetLayoutBinding[0].binding = 0;
        objectDescriptorSetLayoutBinding[0].descriptorCount = 1;
        objectDescriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        objectDescriptorSetLayoutBinding[0].pImmutableSamplers = 0;
        objectDescriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Sampler
        objectDescriptorSetLayoutBinding[1].binding = 1;
        objectDescriptorSetLayoutBinding[1].descriptorCount = 1;
        objectDescriptorSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        objectDescriptorSetLayoutBinding[1].pImmutableSamplers = 0;
        objectDescriptorSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo localDescriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        localDescriptorSetLayoutCreateInfo.bindingCount = sizeof(objectDescriptorSetLayoutBinding) / sizeof(objectDescriptorSetLayoutBinding[0]);
        localDescriptorSetLayoutCreateInfo.pBindings = objectDescriptorSetLayoutBinding;

        KRAFT_VK_CHECK(vkCreateDescriptorSetLayout(context->LogicalDevice.Handle, &localDescriptorSetLayoutCreateInfo, context->AllocationCallbacks, &TestSceneState.LocalDescriptorSetLayout));
    }

    VkDescriptorSetLayout descriptorSetLayouts[] = {
        TestSceneState.GlobalDescriptorSetLayout,
        TestSceneState.LocalDescriptorSetLayout
    };

    VulkanPipelineDescription pipelineDesc = {};
    pipelineDesc.IsWireframe = false;
    pipelineDesc.ShaderStageCount = sizeof(stages) / sizeof(stages[0]);
    pipelineDesc.ShaderStages = stages;
    pipelineDesc.Viewport = viewport;
    pipelineDesc.Scissor = scissor;
    pipelineDesc.Attributes = inputAttributeDesc;
    pipelineDesc.AttributeCount = sizeof(inputAttributeDesc) / sizeof(inputAttributeDesc[0]);
    pipelineDesc.DescriptorSetLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
    pipelineDesc.DescriptorSetLayouts = descriptorSetLayouts;

    ObjectState.Pipeline = {};
    VulkanCreateGraphicsPipeline(context, &context->MainRenderPass, pipelineDesc, &ObjectState.Pipeline);
    assert(ObjectState.Pipeline.Handle);

    VulkanDestroyShaderModule(context, &vertex);
    VulkanDestroyShaderModule(context, &fragment);

    // Create a buffer where we can push camera data
    // As per https://community.arm.com/arm-community-blogs/b/graphics-gaming-and-vr-blog/posts/vulkan-descriptor-and-buffer-management
    // We should ideally be creating 1 VkBuffer per frame
    uint32 extraMemoryFlags = context->PhysicalDevice.SupportsDeviceLocalHostVisible ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
    VulkanCreateBuffer(context, sizeof(GlobalUniformBuffer) * 3, VK_SHARING_MODE_EXCLUSIVE, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | extraMemoryFlags,
        true, &TestSceneState.GlobalUniformBuffer);

    assert(TestSceneState.GlobalUniformBuffer.Handle);

    // Allocate descriptor sets
    // Global descriptor sets
    {
        VkDescriptorPoolSize descriptorPoolSize = {};
        descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorPoolSize.descriptorCount = context->Swapchain.ImageCount;

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        descriptorPoolCreateInfo.poolSizeCount = 1;
        descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
        descriptorPoolCreateInfo.maxSets = context->Swapchain.ImageCount;

        vkCreateDescriptorPool(context->LogicalDevice.Handle, &descriptorPoolCreateInfo, context->AllocationCallbacks, &TestSceneState.GlobalDescriptorPool);

        VkDescriptorSetLayout descriptorSets[3] = {
            TestSceneState.GlobalDescriptorSetLayout,
            TestSceneState.GlobalDescriptorSetLayout,
            TestSceneState.GlobalDescriptorSetLayout,
        };

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        descriptorSetAllocateInfo.descriptorPool = TestSceneState.GlobalDescriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = sizeof(descriptorSets) / sizeof(descriptorSets[0]);
        descriptorSetAllocateInfo.pSetLayouts = descriptorSets;

        KRAFT_VK_CHECK(vkAllocateDescriptorSets(context->LogicalDevice.Handle, &descriptorSetAllocateInfo, TestSceneState.GlobalDescriptorSets));
    }

    // Local buffer for uploading object data
    VulkanCreateBuffer(context, sizeof(ObjectUniformBuffer) * 3, VK_SHARING_MODE_EXCLUSIVE, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | extraMemoryFlags,
        true, &TestSceneState.LocalUniformBuffer);

    // Local descriptor sets
    // These descriptor sets are per object!
    {
        VkDescriptorPoolSize descriptorPoolSizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MAX_OBJECT_COUNT * context->Swapchain.ImageCount },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_OBJECT_COUNT * context->Swapchain.ImageCount },
        };

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        descriptorPoolCreateInfo.poolSizeCount = sizeof(descriptorPoolSizes) / sizeof(descriptorPoolSizes[0]);
        descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
        descriptorPoolCreateInfo.maxSets = context->Swapchain.ImageCount;

        vkCreateDescriptorPool(context->LogicalDevice.Handle, &descriptorPoolCreateInfo, context->AllocationCallbacks, &TestSceneState.LocalDescriptorPool);

        ObjectState.AcquireResources(context);
    }
    
    // #5865f2
    // Vec3f color = Vec3f(0x58 / 255.0f, 0x65 / 255.0f, 0xf2 / 255.0f);
    Vec4f color = KRGBA(0x58, 0x65, 0xf2, 0x1f);
    Vec4f colorB = KRGBA(0xff, 0x6b, 0x6b, 0xff);
    // Vertex and index buffers
    Vertex3D verts[] = {
        {Vec3f(+0.5f, +0.5f, +0.0f), {1.f, 1.f}},
        {Vec3f(-0.5f, -0.5f, +0.0f), {0.f, 0.f}},
        {Vec3f(+0.5f, -0.5f, +0.0f), {1.f, 0.f}},
        {Vec3f(-0.5f, +0.5f, +0.0f), {0.f, 1.f}},
    };

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
    // if (ObjectState.SceneCamera.Dirty)
    {
        TestSceneState.GlobalUBO.View = TestSceneState.SceneCamera.GetViewMatrix();
        VulkanLoadDataInBuffer(context, &TestSceneState.GlobalUniformBuffer, &TestSceneState.GlobalUBO, sizeof(GlobalUniformBuffer), sizeof(GlobalUniformBuffer) * context->CurrentSwapchainImageIndex);

        VkDescriptorBufferInfo descriptorBufferInfo;
        descriptorBufferInfo.buffer = TestSceneState.GlobalUniformBuffer.Handle;
        descriptorBufferInfo.offset = sizeof(GlobalUniformBuffer) * context->CurrentSwapchainImageIndex;
        descriptorBufferInfo.range = sizeof(GlobalUniformBuffer);

        VkWriteDescriptorSet descriptorWriteInfo = {};
        descriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWriteInfo.descriptorCount = 1;
        descriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWriteInfo.dstSet = TestSceneState.GlobalDescriptorSets[context->CurrentSwapchainImageIndex];
        descriptorWriteInfo.dstBinding = 0;
        descriptorWriteInfo.dstArrayElement = 0;
        descriptorWriteInfo.pBufferInfo = &descriptorBufferInfo;

        vkUpdateDescriptorSets(context->LogicalDevice.Handle, 1, &descriptorWriteInfo, 0, 0);
        vkCmdBindDescriptorSets(buffer->Handle, VK_PIPELINE_BIND_POINT_GRAPHICS, ObjectState.Pipeline.Layout, 0, 1, &TestSceneState.GlobalDescriptorSets[context->CurrentSwapchainImageIndex], 0, 0);
    }
    
    // Update objects
    {
        int bindingIndex = 0;
        VkWriteDescriptorSet writeInfos[KRAFT_VULKAN_SHADER_MAX_BINDINGS] = {};

        // Material data
        if (ObjectState.DescriptorSetStates[bindingIndex].Generations[context->CurrentSwapchainImageIndex] == KRAFT_INVALID_ID_UINT8)
        {
            VulkanLoadDataInBuffer(context, &TestSceneState.LocalUniformBuffer, &ObjectState.UBO, sizeof(ObjectUniformBuffer), 0);

            VkDescriptorBufferInfo descriptorBufferInfo;
            descriptorBufferInfo.buffer = TestSceneState.LocalUniformBuffer.Handle;
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range = sizeof(ObjectUniformBuffer);

            VkWriteDescriptorSet descriptorWriteInfo = {};
            descriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWriteInfo.descriptorCount = 1;
            descriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWriteInfo.dstSet = ObjectState.DescriptorSets[context->CurrentSwapchainImageIndex];
            descriptorWriteInfo.dstBinding = bindingIndex;
            descriptorWriteInfo.dstArrayElement = 0;
            descriptorWriteInfo.pBufferInfo = &descriptorBufferInfo;

            writeInfos[bindingIndex] = descriptorWriteInfo;
            ObjectState.DescriptorSetStates[bindingIndex].Generations[context->CurrentSwapchainImageIndex]++;
            
            bindingIndex++;
        }

        // Sampler
        if (ObjectState.DescriptorSetStates[bindingIndex].Generations[context->CurrentSwapchainImageIndex] == KRAFT_INVALID_ID_UINT8)
        {
            VulkanTexture* texture = (VulkanTexture*)ObjectState.Texture->RendererData;
            assert(texture->Image.Handle);
            assert(texture->Image.View);

            VkDescriptorImageInfo descriptorImageInfo = {};
            descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo.imageView = texture->Image.View;
            descriptorImageInfo.sampler = texture->Sampler;

            VkWriteDescriptorSet descriptorWriteInfo = {};
            descriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWriteInfo.descriptorCount = 1;
            descriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWriteInfo.dstSet = ObjectState.DescriptorSets[context->CurrentSwapchainImageIndex];
            descriptorWriteInfo.dstBinding = bindingIndex;
            descriptorWriteInfo.dstArrayElement = 0;
            descriptorWriteInfo.pImageInfo = &descriptorImageInfo;

            writeInfos[bindingIndex] = descriptorWriteInfo;

            ObjectState.DescriptorSetStates[bindingIndex].Generations[context->CurrentSwapchainImageIndex]++;
            bindingIndex++;
        }
        
        if (bindingIndex > 0)
        {
            vkUpdateDescriptorSets(context->LogicalDevice.Handle, bindingIndex, writeInfos, 0, 0);
        }

        // Since this descriptor set is at index 1 in the pipeline layout,
        // firstSet is 1
        vkCmdBindDescriptorSets(buffer->Handle, VK_PIPELINE_BIND_POINT_GRAPHICS, ObjectState.Pipeline.Layout, 1, 1, &ObjectState.DescriptorSets[context->CurrentSwapchainImageIndex], 0, 0);
    }

    // vkCmdDraw(buffer->Handle, 3, 1, 0, 0);
    vkCmdDrawIndexed(buffer->Handle, IndexCount, 1, 0, 0, 0);
}

void DestroyTestScene(VulkanContext* context)
{
    ObjectState.ReleaseResources(context);

    VulkanDestroyBuffer(context, &TestSceneState.LocalUniformBuffer);
    VulkanDestroyBuffer(context, &TestSceneState.GlobalUniformBuffer);
    VulkanDestroyBuffer(context, &VertexBuffer);
    VulkanDestroyBuffer(context, &IndexBuffer);

    VulkanDestroyPipeline(context, &ObjectState.Pipeline);
    ObjectState.Pipeline = {};

    vkDestroyDescriptorPool(context->LogicalDevice.Handle, TestSceneState.LocalDescriptorPool, context->AllocationCallbacks);
    vkDestroyDescriptorSetLayout(context->LogicalDevice.Handle, TestSceneState.LocalDescriptorSetLayout, context->AllocationCallbacks);

    vkDestroyDescriptorPool(context->LogicalDevice.Handle, TestSceneState.GlobalDescriptorPool, context->AllocationCallbacks);
    vkDestroyDescriptorSetLayout(context->LogicalDevice.Handle, TestSceneState.GlobalDescriptorSetLayout, context->AllocationCallbacks);
}

}