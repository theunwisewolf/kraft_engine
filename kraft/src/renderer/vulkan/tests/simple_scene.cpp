#include "simple_scene.h"

#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_string.h"
#include "core/kraft_time.h"
#include "core/kraft_application.h"
#include "core/kraft_imgui.h"
#include "platform/kraft_filesystem.h"

#include "systems/kraft_texture_system.h"
#include "renderer/kraft_renderer_types.h"
#include "renderer/vulkan/kraft_vulkan_shader.h"
#include "renderer/vulkan/kraft_vulkan_buffer.h"
#include "renderer/vulkan/kraft_vulkan_vertex_buffer.h"
#include "renderer/vulkan/kraft_vulkan_index_buffer.h"
#include "renderer/vulkan/kraft_vulkan_pipeline.h"
#include "renderer/vulkan/kraft_vulkan_texture.h"
#include "renderer/kraft_renderer_frontend.h"

#include <imgui.h>

#define MAX_OBJECT_COUNT 128

namespace kraft
{

static VulkanBuffer VertexBuffer;
static VulkanBuffer IndexBuffer;
static uint32 IndexCount = 0;
static SimpleObjectState ObjectState;
static void* TextureID;

static SceneState TestSceneState = {};
static char TextureName[] = "res/textures/test-vert-image-2.jpg";
static char TextureNameWide[] = "res/textures/test-wide-image-1.jpg";

bool KeyDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    static bool defaultTexture = false;
    kraft::Keys keycode = (kraft::Keys)data.Int32[0];
    if (keycode == kraft::KEY_C)
    {
        Texture *oldTexture = ObjectState.Texture;
        ObjectState.Texture = defaultTexture ? TextureSystem::AcquireTexture(TextureName) : TextureSystem::AcquireTexture(TextureNameWide);
        ObjectState.Dirty = true;
        TextureSystem::ReleaseTexture(oldTexture);

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

bool ScrollEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    Camera& camera = TestSceneState.SceneCamera;
    float y = data.Float64[1];
    float32 speed = 50.f;

    if (TestSceneState.Projection == SceneState::ProjectionType::Perspective)
    {
        kraft::Vec3f direction = ForwardVector(camera.ViewMatrix);
        direction = direction * y;

        kraft::Vec3f position = camera.Position;
        kraft::Vec3f change = direction * speed * (float32)Time::DeltaTime;
        camera.SetPosition(position + change);
        camera.Dirty = true;
    }
    else
    {
        float zoom = 1.f;
        if (y >= 0)
        {
            zoom *= 1.1f;
        }
        else
        {
            zoom /= 1.1f;
        }

        ObjectState.Scale *= zoom;
        ObjectState.ModelMatrix = ScaleMatrix(ObjectState.Scale) * RotationMatrixFromEulerAngles(ObjectState.Rotation) * TranslationMatrix(ObjectState.Position);
    }

    return false;
}

bool OnDragDrop(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int count = (int)data.Int64[0];
    const char **paths = (const char**)data.Int64[1];

    // Try loading the texture from the command line args
    const char *filePath = paths[count - 1];
    if (kraft::filesystem::FileExists(filePath))
    {
        KINFO("Loading texture from path %s", filePath);
        Texture *oldTexture = ObjectState.Texture;
        ObjectState.Texture = TextureSystem::AcquireTexture(filePath);
        ObjectState.Dirty = true;
        TextureSystem::ReleaseTexture(oldTexture);

        for (int i = 0; i < KRAFT_VULKAN_SHADER_MAX_BINDINGS; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                ObjectState.DescriptorSetStates[i].Generations[j] = KRAFT_INVALID_ID_UINT8;
            }
        }
    }
    else
    {
        KWARN("%s path does not exist", filePath)
    }

    return false;
}

bool MouseDragStartEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int x = data.Int32[0];
    int y = data.Int32[1];
    KINFO("Drag started = %d, %d", x, y);

    return false;
}

bool MouseDragDraggingEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    MousePosition MousePositionPrev = InputSystem::GetPreviousMousePosition();
    int x = data.Int32[0] - MousePositionPrev.x;
    int y = data.Int32[1] - MousePositionPrev.y;
    // KINFO("Delta position = %d, %d", x, y);

    return false;
}

bool MouseDragEndEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int x = data.Int32[0];
    int y = data.Int32[1];
    KINFO("Drag ended at = %d, %d", x, y);

    return false;
}

SceneState* GetSceneState()
{
    return &TestSceneState;
}

static void ImGuiWidgets(bool refresh)
{
    ImGui::Begin("Debug");
    ObjectState.Dirty |= refresh;

    static float    left   = -(float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
                    right  = (float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
                    top    = -(float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
                    bottom = (float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
                    nearClip = -1.f, 
                    farClip = 1.f;

    static float fov = 45.f;
    static float width = (float)kraft::Application::Get()->Config.WindowWidth;
    static float height = (float)kraft::Application::Get()->Config.WindowHeight;
    static float nearClipP = 0.1f;
    static float farClipP = 1000.f;

    // To preserve the aspect ratio of the texture
    kraft::Vec2f ratio = { (float)ObjectState.Texture->Width / Application::Get()->Config.WindowWidth, (float)ObjectState.Texture->Height / Application::Get()->Config.WindowHeight };
    float downScale = kraft::math::Max(ratio.x, ratio.y);

    static Vec3f rotationDeg = Vec3fZero;
    static bool usePerspectiveProjection = TestSceneState.Projection == SceneState::ProjectionType::Perspective;

    kraft::Camera& camera = TestSceneState.SceneCamera;
    if (ImGui::RadioButton("Perspective Projection", usePerspectiveProjection == true) || (ObjectState.Dirty && usePerspectiveProjection))
    {
        usePerspectiveProjection = true;
        SetProjection(SceneState::ProjectionType::Perspective);

        ObjectState.Scale = {(float)ObjectState.Texture->Width / 20.f / downScale, (float)ObjectState.Texture->Height / 20.f / downScale, 1.0f};
        ObjectState.Dirty = true;
    }

    if (ImGui::RadioButton("Orthographic Projection", usePerspectiveProjection == false) || (ObjectState.Dirty && !usePerspectiveProjection))
    {
        usePerspectiveProjection = false;
        SetProjection(SceneState::ProjectionType::Orthographic);

        ObjectState.Scale = {(float)ObjectState.Texture->Width / downScale, (float)ObjectState.Texture->Height / downScale, 1.0f};
        ObjectState.Dirty = true;
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
        if (ImGui::DragFloat4("Translation", camera.Position))
        {
            camera.Dirty = true;
        }
    }
    ImGui::PopID();

    ImGui::PushID("OBJECT_TRANSFORM");
    {
        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::Separator();
        
        if (ImGui::DragFloat3("Scale", ObjectState.Scale._data))
        {
            KINFO("Scale - %f, %f, %f", ObjectState.Scale.x, ObjectState.Scale.y, ObjectState.Scale.z);
            ObjectState.Dirty = true;
            // ObjectState.ModelMatrix = ScaleMatrix(Vec3f{scale.x, scale.y, scale.z});
        }

        if (ImGui::DragFloat3("Translation", ObjectState.Position._data))
        {
            KINFO("Translation - %f, %f, %f", ObjectState.Position.x, ObjectState.Position.y, ObjectState.Position.z);
            ObjectState.Dirty = true;
            // ObjectState.ModelMatrix *= TranslationMatrix(position);
        }

        if (ImGui::DragFloat3("Rotation Degrees", rotationDeg._data))
        {
            ObjectState.Rotation.x = DegToRadians(rotationDeg.x);
            ObjectState.Rotation.y = DegToRadians(rotationDeg.y);
            ObjectState.Rotation.z = DegToRadians(rotationDeg.z);
            ObjectState.Dirty = true;
        }
    }
    ImGui::PopID();

    if (ObjectState.Dirty)
    {
        ObjectState.ModelMatrix = ScaleMatrix(ObjectState.Scale) * RotationMatrixFromEulerAngles(ObjectState.Rotation) * TranslationMatrix(ObjectState.Position);
    }

    ImVec2 ViewPortPanelSize = ImGui::GetContentRegionAvail();
        //     {Vec3f(+0.5f, +0.5f, +0.0f), {1.f, 1.f}},
        // {Vec3f(-0.5f, -0.5f, +0.0f), {0.f, 0.f}},
        // {Vec3f(+0.5f, -0.5f, +0.0f), {1.f, 0.f}},
        // {Vec3f(-0.5f, +0.5f, +0.0f), {0.f, 1.f}},
    ImGui::Image(TextureID, ViewPortPanelSize, {0,1}, {1,0});

    ImGui::End();

    ObjectState.Dirty = false;
}

void SimpleObjectState::AcquireResources(VulkanContext *context)
{
    VkDescriptorSetLayout descriptorSets[] = 
    {
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
    // TextureSystem::ReleaseTexture(TextureName);
    // KRAFT_VK_CHECK(vkFreeDescriptorSets(context->LogicalDevice.Handle, TestSceneState.LocalDescriptorPool, 3, ObjectState.DescriptorSets));

    for (int i = 0; i < KRAFT_VULKAN_SHADER_MAX_BINDINGS; ++i)
    {
        for (int j = 0; j < 3; j++)
            ObjectState.DescriptorSetStates[i].Generations[j] = -1;
    }
}

struct OffscreenRenderPass
{

};

void InitTestScene(VulkanContext* context)
{
    EventSystem::Listen(EVENT_TYPE_KEY_DOWN, &TestSceneState, KeyDownEventListener);
    EventSystem::Listen(EVENT_TYPE_SCROLL, &TestSceneState, ScrollEventListener);
    EventSystem::Listen(EVENT_TYPE_WINDOW_DRAG_DROP, &TestSceneState, OnDragDrop);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_START, &TestSceneState, MouseDragStartEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_DRAGGING, &TestSceneState, MouseDragDraggingEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_END, &TestSceneState, MouseDragEndEventListener);

    // Load textures
    if (kraft::Application::Get()->CommandLineArgs.Count > 1)
    {
        // Try loading the texture from the command line args
        String FilePath = kraft::Application::Get()->CommandLineArgs.Arguments[1];
        if (kraft::filesystem::FileExists(FilePath))
        {
            KINFO("Loading texture from cli path %s", FilePath.Data());
            ObjectState.Texture = TextureSystem::AcquireTexture(FilePath.Data());
        }
    }

    if (!ObjectState.Texture)
        ObjectState.Texture = TextureSystem::AcquireTexture(TextureName);
    // ObjectState.Texture = TextureSystem::GetDefaultDiffuseTexture();

    SetProjection(SceneState::ProjectionType::Orthographic);
    
    // To preserve the aspect ratio of the texture
    kraft::Vec2f ratio = { (float)ObjectState.Texture->Width / Application::Get()->Config.WindowWidth, (float)ObjectState.Texture->Height / Application::Get()->Config.WindowHeight };
    float downScale = kraft::math::Max(ratio.x, ratio.y);
    if (TestSceneState.Projection == SceneState::ProjectionType::Orthographic)
    {
        ObjectState.Scale = {(float)ObjectState.Texture->Width / downScale, (float)ObjectState.Texture->Height / downScale, 1.0f};
    }
    else
    {
        ObjectState.Scale = {(float)ObjectState.Texture->Width / 20.f / downScale, (float)ObjectState.Texture->Height / 20.f / downScale, 1.0f};
    }

    ObjectState.ModelMatrix = ScaleMatrix(ObjectState.Scale);
    ObjectState.UBO.DiffuseColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    TestSceneState.GlobalUBO.View = TestSceneState.SceneCamera.GetViewMatrix();
    TestSceneState.SceneCamera.Dirty = true;
    ObjectState.Dirty = true;

    // VulkanCreateDefaultTexture(context, 256, 256, 4, &ObjectState.Texture);

    TextureID = Renderer->ImGuiRenderer.AddTexture(ObjectState.Texture);
    Renderer->ImGuiRenderer.AddWidget("Debug", ImGuiWidgets);

    VkShaderModule vertex, fragment;
    char filepath[256] = {};
    kraft::StringFormat(filepath, sizeof(filepath), "%s/res/shaders/basic.kfx.vert.spv", kraft::Application::Get()->BasePath.Data());
    VulkanCreateShaderModule(context, filepath, &vertex);
    KASSERT(vertex);

    kraft::StringFormat(filepath, sizeof(filepath), "%s/res/shaders/basic.kfx.frag.spv", kraft::Application::Get()->BasePath.Data());
    VulkanCreateShaderModule(context, filepath, &fragment);
    KASSERT(fragment);

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
        // globalUniformObject
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
        // LocalUniformObject
        // Currently only diffuse color
        objectDescriptorSetLayoutBinding[0].binding = 0;
        objectDescriptorSetLayoutBinding[0].descriptorCount = 1;
        objectDescriptorSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        objectDescriptorSetLayoutBinding[0].pImmutableSamplers = 0;
        objectDescriptorSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // diffuseSampler
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

    VkDescriptorSetLayout descriptorSetLayouts[] = 
    {
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
    KASSERT(ObjectState.Pipeline.Handle);

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

    KASSERT(TestSceneState.GlobalUniformBuffer.Handle);

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
        VkDescriptorPoolSize descriptorPoolSizes[] = 
        {
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

    UpdateDescriptorSets(context);
}

// Uploads the data into the buffers and assigns the buffers
// to the corresponding descriptor set
void UpdateDescriptorSets(VulkanContext *context)
{
    TestSceneState.GlobalUBO.View = TestSceneState.SceneCamera.GetViewMatrix();
    // Update all the descriptor sets
    for (int i = 0; i < 3; i++)
    {
        VulkanLoadDataInBuffer(context, &TestSceneState.GlobalUniformBuffer, &TestSceneState.GlobalUBO, sizeof(GlobalUniformBuffer), sizeof(GlobalUniformBuffer) * i);
        VkDescriptorBufferInfo descriptorBufferInfo;
        descriptorBufferInfo.buffer = TestSceneState.GlobalUniformBuffer.Handle;
        descriptorBufferInfo.offset = sizeof(GlobalUniformBuffer) * i;
        descriptorBufferInfo.range = sizeof(GlobalUniformBuffer);

        VkWriteDescriptorSet descriptorWriteInfo = {};
        descriptorWriteInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWriteInfo.descriptorCount = 1;
        descriptorWriteInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWriteInfo.dstSet = TestSceneState.GlobalDescriptorSets[i];
        descriptorWriteInfo.dstBinding = 0;
        descriptorWriteInfo.dstArrayElement = 0;
        descriptorWriteInfo.pBufferInfo = &descriptorBufferInfo;

        vkUpdateDescriptorSets(context->LogicalDevice.Handle, 1, &descriptorWriteInfo, 0, 0);
    }
}

void RenderTestScene(VulkanContext* context, VulkanCommandBuffer* buffer)
{
    VulkanBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, &ObjectState.Pipeline);
    
    vkCmdPushConstants(buffer->Handle, ObjectState.Pipeline.Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ObjectState.ModelMatrix), &ObjectState.ModelMatrix);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(buffer->Handle, 0, 1, &VertexBuffer.Handle, offsets);
    vkCmdBindIndexBuffer(buffer->Handle, IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);

    TestSceneState.GlobalUBO.View = TestSceneState.SceneCamera.GetViewMatrix();
    VulkanLoadDataInBuffer(context, &TestSceneState.GlobalUniformBuffer, &TestSceneState.GlobalUBO, sizeof(GlobalUniformBuffer), sizeof(GlobalUniformBuffer) * context->CurrentSwapchainImageIndex);

    vkCmdBindDescriptorSets(buffer->Handle, VK_PIPELINE_BIND_POINT_GRAPHICS, ObjectState.Pipeline.Layout, 0, 1, &TestSceneState.GlobalDescriptorSets[context->CurrentSwapchainImageIndex], 0, 0);
    
    // Update objects
    {
        int bindingIndex = 0;
        VkWriteDescriptorSet writeInfos[KRAFT_VULKAN_SHADER_MAX_BINDINGS] = {};
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        VkDescriptorImageInfo descriptorImageInfo = {};
        VkWriteDescriptorSet descriptorWriteInfo = {};

        // Material data
        if (ObjectState.DescriptorSetStates[bindingIndex].Generations[context->CurrentSwapchainImageIndex] == KRAFT_INVALID_ID_UINT8)
        {
            VulkanLoadDataInBuffer(context, &TestSceneState.LocalUniformBuffer, &ObjectState.UBO, sizeof(ObjectUniformBuffer), 0);

            descriptorBufferInfo.buffer = TestSceneState.LocalUniformBuffer.Handle;
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range = sizeof(ObjectUniformBuffer);

            descriptorWriteInfo = {};
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

            descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            descriptorImageInfo.imageView = ((VulkanTexture*)ObjectState.Texture->RendererData)->Image.View;
            descriptorImageInfo.sampler = texture->Sampler;

            descriptorWriteInfo = {};
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

void SetProjection(SceneState::ProjectionType projection)
{
    kraft::Camera& camera = TestSceneState.SceneCamera;
    camera.Reset();
    if (projection == SceneState::ProjectionType::Perspective)
    {
        TestSceneState.Projection = SceneState::ProjectionType::Perspective;
        TestSceneState.GlobalUBO.Projection = PerspectiveMatrix(DegToRadians(45.0f), (float)kraft::Application::Get()->Config.WindowWidth / (float)kraft::Application::Get()->Config.WindowHeight, 0.1f, 1000.f);

        camera.SetPosition({0.0f, 0.0f, 30.f});
    }
    else
    {
        TestSceneState.Projection = SceneState::ProjectionType::Orthographic;
        TestSceneState.GlobalUBO.Projection = OrthographicMatrix(
            -(float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
            (float)kraft::Application::Get()->Config.WindowWidth * 0.5f, 
            -(float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
            (float)kraft::Application::Get()->Config.WindowHeight * 0.5f, 
            -1.0f, 1.0f
        );

        camera.SetPosition(Vec3fZero);
    }

    camera.Dirty = true;
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