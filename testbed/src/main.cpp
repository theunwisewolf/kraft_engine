#include <stdio.h>

// #define GLAD_VULKAN_IMPLEMENTATION
// #include <glad/vulkan.h>

#include "core/kraft_asserts.h"
#include "core/kraft_main.h"
#include "core/kraft_memory.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_string.h"
#include "core/kraft_time.h"
#include "math/kraft_math.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_filesystem.h"
#include "renderer/kraft_renderer_types.h"
#include "systems/kraft_material_system.h"
#include "systems/kraft_texture_system.h"
#include "systems/kraft_geometry_system.h"
#include "renderer/shaderfx/kraft_shaderfx.h"

#include <imgui.h>

#include "scenes/simple_scene.h"
#include "widgets.h"
#include "utils.h"

#include <systems/kraft_asset_database.h>

static char TextureName[] = "res/textures/test-vert-image-2.jpg";
static char TextureNameWide[] = "res/textures/test-wide-image-1.jpg";

bool OnDragDrop(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int count = (int)data.Int64Value[0];
    const char **paths = (const char**)data.Int64Value[1];

    // Try loading the texture from the command line args
    const char *TexturePath = paths[count - 1];
    if (kraft::filesystem::FileExists(TexturePath))
    {
        KINFO("Loading texture from path %s", TexturePath);
        kraft::MaterialSystem::SetTexture(TestSceneState->GetSelectedEntity().MaterialInstance, "DiffuseSampler", TexturePath);
        UpdateObjectScale();
    }
    else
    {
        KWARN("%s path does not exist", TexturePath)
    }

    return false;
}

bool KeyDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::Keys KeyCode = (kraft::Keys)data.Int32Value[0];
    KINFO("%s key pressed (code = %d)", kraft::Platform::GetKeyName(KeyCode), KeyCode);

    static bool DefaultTexture = false;
    if (KeyCode == kraft::KEY_C)
    {
        kraft::String TexturePath = DefaultTexture ? TextureName : TextureNameWide;
        kraft::MaterialSystem::SetTexture(TestSceneState->GetSelectedEntity().MaterialInstance, "DiffuseSampler", TexturePath);
        DefaultTexture = !DefaultTexture;
        UpdateObjectScale();
    }

    return false;
}

bool KeyUpEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::Keys keycode = (kraft::Keys)data.Int32Value[0];
    KINFO("%s key released (code = %d)", kraft::Platform::GetKeyName(keycode), keycode);

    return false;
}

bool MouseMoveEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    // int x = data.Int32[0];
    // int y = data.Int32[1];
    // KINFO("Mouse moved = %d, %d", x, y);

    return false;
}

bool MouseDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int button = data.Int32Value[0];
    KINFO("%d mouse button pressed", button);

    return false;
}

bool MouseUpEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int button = data.Int32Value[0];
    KINFO("%d mouse button released", button);

    return false;
}

bool MouseDragStartEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int x = data.Int32Value[0];
    int y = data.Int32Value[1];
    // KINFO("Drag started = %d, %d", x, y);

    return false;
}

bool MouseDragDraggingEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::Camera& Camera = TestSceneState->SceneCamera;
    if (kraft::InputSystem::IsMouseButtonDown(kraft::MouseButtons::MOUSE_BUTTON_RIGHT))
    {
        const float32 Sensitivity = 0.1f;
        kraft::MousePosition MousePositionPrev = kraft::InputSystem::GetPreviousMousePosition();
        float32 x = float32(data.Int32Value[0] - MousePositionPrev.x) * Sensitivity;
        float32 y = -float32(data.Int32Value[1] - MousePositionPrev.y) * Sensitivity;

        Camera.Yaw += x;
        Camera.Pitch += y;
        Camera.Pitch = kraft::math::Clamp(Camera.Pitch, -89.0f, 89.0f);

        Camera.UpdateVectors();
    }

    return false;
}

bool MouseDragEndEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int x = data.Int32Value[0];
    int y = data.Int32Value[1];
    // KINFO("Drag ended at = %d, %d", x, y);

    return false;
}

bool ScrollEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    // kraft::Camera& Camera = TestSceneState->SceneCamera;

    // float y = data.Float64Value[1];
    // float32 speed = 50.f;

    // if (Camera.ProjectionType == kraft::CameraProjectionType::Perspective)
    // {
    //     kraft::Vec3f direction = kraft::ForwardVector(Camera.ViewMatrix);
    //     direction = direction * y;

    //     kraft::Vec3f position = Camera.Position;
    //     kraft::Vec3f change = direction * speed * (float32)kraft::Time::DeltaTime;
    //     Camera.SetPosition(position + change);
    //     Camera.Dirty = true;
    // }
    // else
    // {
    //     float zoom = 1.f;
    //     if (y >= 0)
    //     {
    //         zoom *= 1.1f;
    //     }
    //     else
    //     {
    //         zoom /= 1.1f;
    //     }

    //     TestSceneState->GetSelectedEntity().Scale *= zoom;
    //     TestSceneState->GetSelectedEntity().ModelMatrix = kraft::ScaleMatrix(TestSceneState->GetSelectedEntity().Scale) * kraft::RotationMatrixFromEulerAngles(TestSceneState->GetSelectedEntity().Rotation) * kraft::TranslationMatrix(TestSceneState->GetSelectedEntity().Position);
    // }

    return false;
}

bool Init()
{
    using namespace kraft;
    EventSystem::Listen(EVENT_TYPE_KEY_DOWN, nullptr, KeyDownEventListener);
    EventSystem::Listen(EVENT_TYPE_KEY_UP, nullptr, KeyUpEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DOWN, nullptr, MouseDownEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_UP, nullptr, MouseUpEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_MOVE, nullptr, MouseMoveEventListener);
    EventSystem::Listen(EVENT_TYPE_SCROLL, nullptr, ScrollEventListener);
    EventSystem::Listen(EVENT_TYPE_WINDOW_DRAG_DROP, nullptr, OnDragDrop);

    // Drag events
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_START, nullptr, MouseDragStartEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_DRAGGING, nullptr, MouseDragDraggingEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_END, nullptr, MouseDragEndEventListener);

    SetupScene();

    // Load textures
    String TexturePath;
    if (kraft::Application::Get()->CommandLineArgs.Count > 1)
    {
        // Try loading the texture from the command line args
        String FilePath = kraft::Application::Get()->CommandLineArgs.Arguments[1];
        if (kraft::filesystem::FileExists(FilePath))
        {
            KINFO("Loading texture from cli path %s", FilePath.Data());
            TexturePath = FilePath;
        }
    }

    // kraft::MeshAsset* VikingRoom = kraft::AssetDatabase::Ptr->LoadMesh("res/meshes/viking_room/viking_room.fbx");
    kraft::MeshAsset* VikingRoom = kraft::AssetDatabase::Ptr->LoadMesh("res/meshes/viking_room.obj");
    KASSERT(VikingRoom);
    // kraft::MeshAsset Dragon = kraft::AssetDatabase::Ptr->LoadMesh("res/meshes/dragon.obj");

    SimpleObjectState EntityA;
    EntityA.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_2d.kmt");
    // EntityA.GeometryID = kraft::GeometrySystem::GetDefaultGeometry()->InternalID;
    EntityA.GeometryID = VikingRoom->Geometry->InternalID;
    EntityA.SetTransform({0.0f, 0.0f, 0.0f}, { kraft::DegToRadians(90.0f), kraft::DegToRadians(-90.0f), kraft::DegToRadians(180.0f) }, {10.0f, 10.0f, 10.0f});
    TestSceneState->AddEntity(EntityA);

    const float ObjectCount = 0.0f;
    const float ObjectWidth = 50.0f;
    const float ObjectHeight = 50.0f;
    const float Spacing = 0.0f;
    for (int i = 0; i < ObjectCount; i++)
    {
        SimpleObjectState Object;
        Object.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_2d.kmt");
        Object.GeometryID = kraft::GeometrySystem::GetDefaultGeometry()->InternalID;

        float x = ((ObjectCount * ObjectWidth + Spacing) * -0.5f) + i * ObjectWidth + Spacing;
        Object.SetTransform({x, 200.0f, 0.0f}, kraft::Vec3fZero, {ObjectWidth, ObjectHeight, 50.0f});
        TestSceneState->AddEntity(Object);
    }

    // SimpleObjectState EntityB;
    // EntityB.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_2d_wireframe.kmt");
    // EntityB.GeometryID = kraft::GeometrySystem::GetDefaultGeometry()->InternalID;
    // EntityB.SetTransform({105.0f, 0.0f, 0.0f}, kraft::Vec3fZero, {200.0f, 200.0f, 200.0f});
    // TestSceneState->AddEntity(EntityB);

    // MaterialSystem::SetTexture(EntityB.MaterialInstance, "DiffuseSampler", TextureSystem::AcquireTexture(TextureNameWide));
    TestSceneState->SelectedObjectIndex = 0;

    SetProjection(kraft::CameraProjectionType::Perspective);
    
    // To preserve the aspect ratio of the texture
    // UpdateObjectScale();

    const kraft::Material* Material = TestSceneState->GetSelectedEntity().MaterialInstance;
    Handle<Texture> DiffuseTexture = Material->GetUniform<Handle<Texture>>("DiffuseSampler");
    InitImguiWidgets(DiffuseTexture);

    return true;
}

void Update(float64 deltaTime)
{
    // KINFO("%f ms", kraft::Platform::GetElapsedTime());
    kraft::Camera& Camera = TestSceneState->SceneCamera;
    // if (!kraft::InputSystem::IsMouseButtonDown(kraft::MouseButtons::MOUSE_BUTTON_RIGHT))
    {
        float32 Speed = 50.f * deltaTime;
        kraft::Vec3f direction = kraft::Vec3fZero;
        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_UP) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_W))
        {
            if (Camera.ProjectionType == kraft::CameraProjectionType::Orthographic)
            {
                kraft::Vec3f upVector = UpVector(Camera.GetViewMatrix());
                direction += upVector;
            }
            else
            {
                Camera.Position += Camera.Front * Speed;
                // kraft::Vec3f forwardVector = ForwardVector(Camera.GetViewMatrix());
                // direction += forwardVector;
            }
        }
        else if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_DOWN) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_S))
        {
            if (Camera.ProjectionType == kraft::CameraProjectionType::Orthographic)
            {
                kraft::Vec3f downVector = DownVector(Camera.GetViewMatrix());
                direction += downVector;
            }
            else
            {
                Camera.Position -= Camera.Front * Speed;
                // kraft::Vec3f backwardVector = BackwardVector(Camera.GetViewMatrix());
                // direction += backwardVector;
            }
        }

        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_LEFT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_A))
        {
            // kraft::Vec3f leftVector = LeftVector(Camera.GetViewMatrix());
            // direction += leftVector;
            Camera.Position -= Camera.Right * Speed;
        }
        else if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_RIGHT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_D))
        {
            // kraft::Vec3f rightVector = RightVector(Camera.GetViewMatrix());
            // direction += rightVector;
            Camera.Position += Camera.Right * Speed;
        }

        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_SPACE))
        {
            if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_LEFT_SHIFT))
            {
                kraft::Vec3f down = DownVector(Camera.GetViewMatrix());
                direction += down;
            }
            else
            {
                kraft::Vec3f up = UpVector(Camera.GetViewMatrix());
                direction += up;
            }
        }

        // if (!Vec3fCompare(direction, kraft::Vec3fZero, 0.00001f))
        // {
        //     direction = kraft::Normalize(direction);
        //     kraft::Vec3f position = Camera.Position;
        //     kraft::Vec3f change = direction * speed * (float32)deltaTime;
        //     Camera.SetPosition(position + change);
        // }
    }
    // else
    // {
    //     if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_UP) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_W))
    //     {
    //         Camera.SetPitch(Camera.Pitch + 1.f * deltaTime);
    //     }

    //     if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_DOWN) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_S))
    //     {
    //         Camera.SetPitch(Camera.Pitch - 1.f * deltaTime);
    //     }

    //     if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_LEFT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_A))
    //     {
    //         Camera.SetYaw(Camera.Yaw + 1.f * deltaTime);
    //     }

    //     if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_RIGHT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_D))
    //     {
    //         Camera.SetYaw(Camera.Yaw - 1.f * deltaTime);
    //     }
    // }
}

void Render(float64 deltaTime, kraft::renderer::RenderPacket& RenderPacket)
{
    kraft::Camera& Camera = TestSceneState->SceneCamera;
    RenderPacket.ProjectionMatrix = Camera.ProjectionMatrix;
    RenderPacket.ViewMatrix = Camera.GetViewMatrix();

    for (uint32 i = 0; i < TestSceneState->ObjectsCount; i++)
    {
        kraft::Renderer->AddRenderable(TestSceneState->ObjectStates[i]);
    }
}

void OnResize(size_t width, size_t height)
{
    KINFO("Window size changed = %d, %d", width, height);
}

void OnBackground()
{

}

void OnForeground()
{

}

bool Shutdown()
{
    DestroyScene();

    return true;
}


bool CreateApplication(kraft::Application* app, int argc, char *argv[])
{
    app->Config.ApplicationName = "Kraft!";
    app->Config.WindowTitle     = "Kraft! [VULKAN]";
    app->Config.WindowWidth     = 1280;
    app->Config.WindowHeight    = 800;
    app->Config.RendererBackend = kraft::renderer::RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN;
    app->Config.ConsoleApp      = false;

    app->Init                   = Init;
    app->Update                 = Update;
    app->Render                 = Render;
    app->OnResize               = OnResize;
    app->OnBackground           = OnBackground;
    app->OnForeground           = OnForeground;
    app->Shutdown               = Shutdown;

    return true;
}

