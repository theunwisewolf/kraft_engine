#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_engine.h>
#include <core/kraft_events.h>
#include <core/kraft_input.h>
#include <core/kraft_log.h>
#include <core/kraft_string.h>
#include <core/kraft_time.h>
#include <math/kraft_math.h>
#include <platform/kraft_filesystem.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include <systems/kraft_asset_database.h>
#include <systems/kraft_geometry_system.h>
#include <systems/kraft_material_system.h>
#include <systems/kraft_shader_system.h>
#include <systems/kraft_texture_system.h>
#include <world/kraft_components.h>
#include <world/kraft_entity.h>
#include <world/kraft_world.h>

#include <renderer/kraft_renderer_types.h>
#include <resources/kraft_resource_types.h>
#include <systems/kraft_asset_types.h>

#include <entt/entt.h>
#include <imgui.h>

#include "editor.h"
#include "imgui/imgui_renderer.h"
#include "utils.h"
#include "widgets.h"

ApplicationState GlobalAppState = {
    .LastTime = 0.0f,
    .WindowTitleBuffer = { 0 },
    .TargetFrameTime = 1 / 144.f,
    .FrameCount = 0,
    .TimeSinceLastFrame = 0.f,
    // .ImGuiRenderer = RendererImGui(),
};

static char TextureName[] = "res/textures/test-vert-image-2.jpg";
static char TextureNameWide[] = "res/textures/test-wide-image-1.jpg";

bool OnDragDrop(kraft::EventType type, void* sender, void* listener, kraft::EventData data)
{
    // int          count = (int)data.Int64Value[0];
    // const char** paths = (const char**)data.Int64Value[1];

    // // Try loading the texture from the command line args
    // const char* TexturePath = paths[count - 1];
    // if (kraft::filesystem::FileExists(TexturePath))
    // {
    //     KINFO("Loading texture from path %s", TexturePath);
    //     kraft::MaterialSystem::SetTexture(TestSceneState->GetSelectedEntity().MaterialInstance, "DiffuseSampler", TexturePath);
    //     UpdateObjectScale(1);
    // }
    // else
    // {
    //     KWARN("%s path does not exist", TexturePath)
    // }

    return false;
}

bool KeyDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data)
{
    // kraft::Keys KeyCode = (kraft::Keys)data.Int32Value[0];
    // KINFO("%s key pressed (code = %d)", kraft::Platform::GetKeyName(KeyCode), KeyCode);

    // static bool DefaultTexture = false;
    // if (KeyCode == kraft::KEY_C)
    // {
    //     kraft::String TexturePath = DefaultTexture ? TextureName : TextureNameWide;
    //     kraft::MaterialSystem::SetTexture(TestSceneState->GetSelectedEntity().MaterialInstance, "DiffuseSampler", TexturePath);
    //     DefaultTexture = !DefaultTexture;
    //     UpdateObjectScale(1);
    // }

    return false;
}

bool KeyUpEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data)
{
    kraft::Keys keycode = (kraft::Keys)data.Int32Value[0];
    // KINFO("%s key released (code = %d)", kraft::Platform::GetKeyName(keycode), keycode);

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
    // KINFO("%d mouse button pressed", button);

    if (button == kraft::MouseButtons::MOUSE_BUTTON_RIGHT)
    {
        kraft::Platform::GetWindow()->SetCursorMode(kraft::CURSOR_MODE_DISABLED);
        kraft::Platform::GetWindow()->SetCursorPosition(0.0f, 0.0f);

        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        IO.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
        EditorState::Ptr->ViewportCameraSettings.Flying = true;
    }

    return false;
}

bool MouseUpEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data)
{
    int button = data.Int32Value[0];
    // KINFO("%d mouse button released", button);

    if (button == kraft::MouseButtons::MOUSE_BUTTON_RIGHT)
    {
        kraft::Platform::GetWindow()->SetCursorMode(kraft::CURSOR_MODE_NORMAL);
        ImGuiIO& IO = ImGui::GetIO();

        IO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        IO.ConfigFlags &= ~ImGuiConfigFlags_NavNoCaptureKeyboard;
        EditorState::Ptr->ViewportCameraSettings.Flying = false;
    }

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
    return false;
    kraft::Camera& Camera = EditorState::Ptr->CurrentWorld->Camera;
    if (EditorState::Ptr->ViewportCameraSettings.Flying)
    {
        // GLFW has a bug where the first call to GetCursorPos returns a very large incorrect value on mac and windows
        if (EditorState::Ptr->ViewportCameraSettings.Flying > 3)
        {
            kraft::MousePosition MousePositionPrev = kraft::InputSystem::GetPreviousMousePosition();
            // float32              X = float32(data.Int32Value[0] - MousePositionPrev.x);
            // float32              Y = -float32(data.Int32Value[1] - MousePositionPrev.y);

            float64 X, Y;
            kraft::Platform::GetWindow()->GetCursorPosition(&X, &Y);

            // KINFO("Dragging = %f, %f", X, Y);
            Y = -Y;
            // X = X * EditorState::Ptr->ViewportCameraSettings.Sensitivity * kraft::math::Min(kraft::Time::DeltaTime, 1 / 144.0);
            // Y = Y * EditorState::Ptr->ViewportCameraSettings.Sensitivity * kraft::math::Min(kraft::Time::DeltaTime, 1 / 144.0);

            X = X * EditorState::Ptr->ViewportCameraSettings.Sensitivity * kraft::Time::DeltaTime;
            Y = Y * EditorState::Ptr->ViewportCameraSettings.Sensitivity * kraft::Time::DeltaTime;

            Camera.Yaw += float32(X);
            Camera.Pitch += float32(Y);
            Camera.Pitch = kraft::math::Clamp(Camera.Pitch, -89.0f, 89.0f);

            Camera.UpdateVectors();
        }
        else
        {
            EditorState::Ptr->ViewportCameraSettings.Flying++;
        }
        kraft::Platform::GetWindow()->SetCursorPosition(0.0f, 0.0f);
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

bool OnResize(kraft::EventType, void*, void*, kraft::EventData Data)
{
    uint32 Width = Data.UInt32Value[0];
    uint32 Height = Data.UInt32Value[1];
    KINFO("Window size changed = %d, %d", Width, Height);

    GlobalAppState.ImGuiRenderer.OnResize(Width, Height);

    return false;
}

bool Init()
{
    EditorState* Editor = new EditorState();
    GlobalAppState.ImGuiRenderer.Init();
    EditorState::Ptr->CurrentWorld = (kraft::World*)kraft::Malloc(sizeof(kraft::World), kraft::MEMORY_TAG_NONE, true);
    new (EditorState::Ptr->CurrentWorld) kraft::World();

    EditorState::Ptr->SetCamera(kraft::Camera());

    using namespace kraft;
    EventSystem::Listen(EVENT_TYPE_KEY_DOWN, nullptr, KeyDownEventListener);
    EventSystem::Listen(EVENT_TYPE_KEY_UP, nullptr, KeyUpEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DOWN, nullptr, MouseDownEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_UP, nullptr, MouseUpEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_MOVE, nullptr, MouseMoveEventListener);
    EventSystem::Listen(EVENT_TYPE_SCROLL, nullptr, ScrollEventListener);
    EventSystem::Listen(EVENT_TYPE_WINDOW_DRAG_DROP, nullptr, OnDragDrop);
    EventSystem::Listen(EVENT_TYPE_WINDOW_RESIZE, nullptr, OnResize);

    // Drag events
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_START, nullptr, MouseDragStartEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_DRAGGING, nullptr, MouseDragDraggingEventListener);
    EventSystem::Listen(EVENT_TYPE_MOUSE_DRAG_END, nullptr, MouseDragEndEventListener);

    // Load textures
    String TexturePath;
    auto&  CliArgs = kraft::Engine::GetCommandLineArgs();
    if (CliArgs.Length > 1)
    {
        // Try loading the texture from the command line args
        String FilePath = CliArgs[1];
        if (kraft::filesystem::FileExists(FilePath))
        {
            KINFO("Loading texture from cli path %s", FilePath.Data());
            TexturePath = FilePath;
        }
    }

    kraft::Entity         WorldRoot = EditorState::Ptr->CurrentWorld->GetRoot();
    kraft::Entity         GlobalLightEntity = EditorState::Ptr->CurrentWorld->CreateEntity("GlobalLight", WorldRoot);
    kraft::LightComponent GlobalLight = GlobalLightEntity.AddComponent<kraft::LightComponent>();
    GlobalLight.LightColor = kraft::Vec4fOne;
    GlobalLightEntity.GetComponent<TransformComponent>().SetPosition({ 38.0f, 38.0f, 54.0f });

    EditorState::Ptr->CurrentWorld->GlobalLight = GlobalLightEntity.EntityHandle;

    kraft::Entity Sprite2D = EditorState::Ptr->CurrentWorld->CreateEntity("Sprite2D", WorldRoot);
    Sprite2D.AddComponent<MeshComponent>(
        kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_2d.kmt", EditorState::Ptr->RenderSurface.RenderPass), kraft::GeometrySystem::GetDefault2DGeometry()->InternalID
    );
    Sprite2D.GetComponent<TransformComponent>().SetScale(50.0f);

    // kraft::MeshAsset* VikingRoom = kraft::AssetDatabase::LoadMesh("res/meshes/viking_room/viking_room.fbx");
    kraft::MeshAsset* VikingRoom = kraft::AssetDatabase::LoadMesh("res/meshes/viking_room.obj");
    KASSERT(VikingRoom);

    kraft::Entity VikingRoomMeshParent = EditorState::Ptr->CurrentWorld->CreateEntity("VikingRoomParent", WorldRoot);
    VikingRoomMeshParent.GetComponent<TransformComponent>().SetTransform(
        kraft::Vec3f{ 0.0f, 0.0f, 16.0f }, kraft::Vec3f{ kraft::DegToRadians(90.0f), kraft::DegToRadians(-90.0f), kraft::DegToRadians(180.0f) }, kraft::Vec3f{ 20.0f, 20.0f, 20.0f }
    );

    EditorState::Ptr->SelectedEntity = VikingRoomMeshParent.EntityHandle;
    for (int i = 0; i < VikingRoom->SubMeshes.Length; i++)
    {
        kraft::Entity  TestMesh = EditorState::Ptr->CurrentWorld->CreateEntity(VikingRoom->NodeHierarchy[VikingRoom->SubMeshes[i].NodeIdx].Name, VikingRoomMeshParent);
        MeshComponent& Mesh = TestMesh.AddComponent<MeshComponent>();
        Mesh.GeometryID = VikingRoom->SubMeshes[i].Geometry->InternalID;
        Mesh.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_3d.kmt", EditorState::Ptr->RenderSurface.RenderPass);

        kraft::TransformComponent& Transform = TestMesh.GetComponent<TransformComponent>();
        VikingRoom->SubMeshes[i].Transform.Decompose(Transform.Position, Transform.Rotation, Transform.Scale);
        Transform.ComputeModelMatrix();
    }

    VikingRoomMeshParent = EditorState::Ptr->CurrentWorld->CreateEntity("VikingRoomParent2", WorldRoot);
    VikingRoomMeshParent.GetComponent<TransformComponent>().SetTransform(
        kraft::Vec3f{ 30.0f, 0.0f, 16.0f }, kraft::Vec3f{ kraft::DegToRadians(90.0f), kraft::DegToRadians(-90.0f), kraft::DegToRadians(180.0f) }, kraft::Vec3f{ 20.0f, 20.0f, 20.0f }
    );
    for (int i = 0; i < VikingRoom->SubMeshes.Length; i++)
    {
        kraft::Entity  TestMesh = EditorState::Ptr->CurrentWorld->CreateEntity(VikingRoom->NodeHierarchy[VikingRoom->SubMeshes[i].NodeIdx].Name, VikingRoomMeshParent);
        MeshComponent& Mesh = TestMesh.AddComponent<MeshComponent>();
        Mesh.GeometryID = VikingRoom->SubMeshes[i].Geometry->InternalID;
        Mesh.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_3d.kmt", EditorState::Ptr->RenderSurface.RenderPass);

        kraft::TransformComponent& Transform = TestMesh.GetComponent<TransformComponent>();
        VikingRoom->SubMeshes[i].Transform.Decompose(Transform.Position, Transform.Rotation, Transform.Scale);
        Transform.ComputeModelMatrix();
    }

    kraft::MeshAsset* Chair = kraft::AssetDatabase::LoadMesh("res/meshes/chair.fbx");
    KASSERT(Chair);
    VikingRoomMeshParent = EditorState::Ptr->CurrentWorld->CreateEntity("Chair", WorldRoot);
    VikingRoomMeshParent.GetComponent<TransformComponent>().SetTransform(
        kraft::Vec3f{ 50.0f, 50.0f, 16.0f }, kraft::Vec3f{ kraft::DegToRadians(90.0f), kraft::DegToRadians(-90.0f), kraft::DegToRadians(180.0f) }, kraft::Vec3f{ 20.0f, 20.0f, 20.0f }
    );
    for (int i = 0; i < VikingRoom->SubMeshes.Length; i++)
    {
        kraft::Entity  TestMesh = EditorState::Ptr->CurrentWorld->CreateEntity(Chair->NodeHierarchy[Chair->SubMeshes[i].NodeIdx].Name, VikingRoomMeshParent);
        MeshComponent& Mesh = TestMesh.AddComponent<MeshComponent>();
        Mesh.GeometryID = Chair->SubMeshes[i].Geometry->InternalID;
        Mesh.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_3d.kmt", EditorState::Ptr->RenderSurface.RenderPass);

        kraft::TransformComponent& Transform = TestMesh.GetComponent<TransformComponent>();
        Chair->SubMeshes[i].Transform.Decompose(Transform.Position, Transform.Rotation, Transform.Scale);
        Transform.ComputeModelMatrix();
    }

#if 0
    Array<kraft::Entity> NodeEntityHierarchy(RogueSkeleton->NodeHierarchy.Length);
    for (int i = 0; i < RogueSkeleton->NodeHierarchy.Length; i++)
    {
        auto&          Node = RogueSkeleton->NodeHierarchy[i];
        kraft::Entity& ParentEntity = Node.ParentNodeIdx == -1 ? MeshParent : NodeEntityHierarchy[Node.ParentNodeIdx];
        kraft::Entity  NodeEntity = EditorState::Ptr->CurrentWorld->CreateEntity(Node.Name, ParentEntity);
        NodeEntityHierarchy[i] = NodeEntity;
        if (Node.MeshIdx != -1)
        {
            kraft::MeshT&  SubMesh = RogueSkeleton->SubMeshes[Node.MeshIdx];
            MeshComponent& Mesh = NodeEntity.AddComponent<MeshComponent>();
            Mesh.GeometryID = SubMesh.Geometry->InternalID;
            Mesh.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_3d.kmt");

            if (SubMesh.Textures.Length > 0)
            {
                MaterialSystem::SetTexture(Mesh.MaterialInstance, "DiffuseSampler", SubMesh.Textures[0]);
            }

            kraft::TransformComponent& Transform = NodeEntity.GetComponent<TransformComponent>();
            ImGuizmo::DecomposeMatrixToComponents(
                SubMesh.Transform, Transform.Position._data, Transform.Rotation._data, Transform.Scale._data
            );
            Transform.Rotation.x = kraft::DegToRadians(Transform.Rotation.x);
            Transform.Rotation.y = kraft::DegToRadians(Transform.Rotation.y);
            Transform.Rotation.z = kraft::DegToRadians(Transform.Rotation.z);
            Transform.ModelMatrix = SubMesh.Transform;
        }
    }
#endif
#if 1
    kraft::MeshAsset* RogueSkeleton = kraft::AssetDatabase::LoadMesh("res/meshes/skeleton_rogue/skeleton_rogue_binary.fbx");
    KASSERT(RogueSkeleton);
    // kraft::MeshAsset Dragon = kraft::AssetDatabase::LoadMesh("res/meshes/dragon.obj");

    kraft::Entity MeshParent = EditorState::Ptr->CurrentWorld->CreateEntity("RogueSkeletonParent", WorldRoot);
    EditorState::Ptr->SelectedEntity = MeshParent.EntityHandle;
    MeshParent.GetComponent<TransformComponent>().SetScale({ 20.0f, 20.0f, 20.0f });
    for (int i = 0; i < RogueSkeleton->SubMeshes.Length; i++)
    {
        kraft::Entity  TestMesh = EditorState::Ptr->CurrentWorld->CreateEntity(RogueSkeleton->NodeHierarchy[RogueSkeleton->SubMeshes[i].NodeIdx].Name, MeshParent);
        MeshComponent& Mesh = TestMesh.AddComponent<MeshComponent>();
        Mesh.GeometryID = RogueSkeleton->SubMeshes[i].Geometry->InternalID;
        Mesh.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_3d.kmt", EditorState::Ptr->RenderSurface.RenderPass);

        if (RogueSkeleton->SubMeshes[i].Textures.Length > 0)
        {
            MaterialSystem::SetTexture(Mesh.MaterialInstance, "DiffuseSampler", RogueSkeleton->SubMeshes[i].Textures[0]);
        }

        kraft::TransformComponent& Transform = TestMesh.GetComponent<TransformComponent>();
        RogueSkeleton->SubMeshes[i].Transform.Decompose(Transform.Position, Transform.Rotation, Transform.Scale);
        Transform.ComputeModelMatrix();
    }
#endif
    // const float ObjectCount = 0.0f;
    // const float ObjectWidth = 50.0f;
    // const float ObjectHeight = 50.0f;
    // const float Spacing = 0.0f;
    // for (int i = 0; i < ObjectCount; i++)
    // {
    //     SimpleObjectState Object;
    //     Object.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_2d.kmt");
    //     Object.GeometryID = kraft::GeometrySystem::GetDefaultGeometry()->InternalID;

    //     float x = ((ObjectCount * ObjectWidth + Spacing) * -0.5f) + i * ObjectWidth + Spacing;
    //     Object.SetTransform({ x, 200.0f, 0.0f }, kraft::Vec3fZero, { ObjectWidth, ObjectHeight, 50.0f });
    //     // TestSceneState->AddEntity(Object);
    // }

    // SimpleObjectState EntityB;
    // EntityB.MaterialInstance = kraft::MaterialSystem::CreateMaterialFromFile("res/materials/simple_2d.kmt");
    // EntityB.GeometryID = kraft::GeometrySystem::GetDefaultGeometry()->InternalID;
    // EntityB.SetTransform({ 0.0f, 0.0f, 0.0f }, kraft::Vec3fZero, { 200.0f, 200.0f, 200.0f });
    // TestSceneState->AddEntity(EntityB);

    // MaterialSystem::SetTexture(EntityB.MaterialInstance, "DiffuseSampler", TextureSystem::AcquireTexture(TextureNameWide));
    SetProjection(kraft::CameraProjectionType::Perspective);

    // To preserve the aspect ratio of the texture
    // UpdateObjectScale(1);

    InitImguiWidgets();

    return true;
}

static kraft::Vec3f ExpDecay(kraft::Vec3f a, kraft::Vec3f b, float32 Decay, float64 DeltaTime)
{
    return b + (a - b) * expf(-Decay * DeltaTime);
}

void Update(float64 deltaTime)
{
    if (!EditorState::Ptr->ViewportCameraSettings.Flying)
        return;

    // KINFO("%f ms", kraft::Platform::GetElapsedTime());
    kraft::Camera& Camera = EditorState::Ptr->CurrentWorld->Camera;
    float32 SpeedBoost = kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_LEFT_SHIFT) ? 2.0f : 1.0f;
    // if (!kraft::InputSystem::IsMouseButtonDown(kraft::MouseButtons::MOUSE_BUTTON_RIGHT))
    {
        float32      Speed = SpeedBoost * 50.f * deltaTime;
        kraft::Vec3f direction = kraft::Vec3fZero;
        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_UP) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_W))
        {
            if (Camera.ProjectionType == kraft::CameraProjectionType::Orthographic)
            {
                // kraft::Vec3f upVector = UpVector(Camera.GetViewMatrix());
                // direction += upVector;
                Camera.Position.z -= Speed;
            }
            else
            {
                Camera.Position += Camera.Front * Speed;
                // Camera.Position = ExpDecay(Camera.Position, Camera.Position + Camera.Front * Speed, 128, deltaTime);
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
            // Camera.Position = ExpDecay(Camera.Position, Camera.Position - Camera.Right * Speed, 128, deltaTime);
        }
        else if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_RIGHT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_D))
        {
            // kraft::Vec3f rightVector = RightVector(Camera.GetViewMatrix());
            // direction += rightVector;
            Camera.Position += Camera.Right * Speed;
            // Camera.Position = ExpDecay(Camera.Position, Camera.Position + Camera.Right * Speed, 128, deltaTime);
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

    // GLFW has a bug where the first call to GetCursorPos returns a very large incorrect value on mac and windows
    if (EditorState::Ptr->ViewportCameraSettings.Flying > 3)
    {
        kraft::MousePosition MousePositionPrev = kraft::InputSystem::GetPreviousMousePosition();
        // float32              X = float32(data.Int32Value[0] - MousePositionPrev.x);
        // float32              Y = -float32(data.Int32Value[1] - MousePositionPrev.y);

        float64 X, Y;
        kraft::Platform::GetWindow()->GetCursorPosition(&X, &Y);

        // KINFO("Dragging = %f, %f", X, Y);
        Y = -Y;
        // X = X * EditorState::Ptr->ViewportCameraSettings.Sensitivity * kraft::math::Min(kraft::Time::DeltaTime, 1 / 144.0);
        // Y = Y * EditorState::Ptr->ViewportCameraSettings.Sensitivity * kraft::math::Min(kraft::Time::DeltaTime, 1 / 144.0);

        X = X * EditorState::Ptr->ViewportCameraSettings.Sensitivity * SpeedBoost * kraft::Time::DeltaTime;
        Y = Y * EditorState::Ptr->ViewportCameraSettings.Sensitivity * SpeedBoost * kraft::Time::DeltaTime;

        Camera.Yaw += float32(X);
        Camera.Pitch += float32(Y);
        Camera.Pitch = kraft::math::Clamp(Camera.Pitch, -89.0f, 89.0f);

        Camera.UpdateVectors();
    }
    else
    {
        EditorState::Ptr->ViewportCameraSettings.Flying++;
    }

    kraft::Platform::GetWindow()->SetCursorPosition(0.0f, 0.0f);
}

void Render()
{
    // kraft::Engine::Renderer.Camera = &TestSceneState->SceneCamera;
    // kraft::Engine::Renderer.CurrentWorld = DefaultWorld;
    // for (uint32 i = 0; i < TestSceneState->ObjectsCount; i++)
    // {
    //     kraft::Engine::Renderer.AddRenderable(TestSceneState->ObjectStates[i]);
    // }

    EditorState::Ptr->RenderSurface.World = EditorState::Ptr->CurrentWorld;
    kraft::Engine::Renderer.BeginRenderSurface(EditorState::Ptr->RenderSurface);
    EditorState::Ptr->CurrentWorld->Render();
    kraft::Engine::Renderer.EndRenderSurface(EditorState::Ptr->RenderSurface);
}

void Shutdown()
{
    kraft::Free(EditorState::Ptr->CurrentWorld, sizeof(kraft::World), kraft::MEMORY_TAG_NONE);
    delete EditorState::Ptr;

    GlobalAppState.ImGuiRenderer.Destroy();
}

void Run()
{
    while (kraft::Engine::Running)
    {
        float64 FrameStartTime = kraft::Platform::GetAbsoluteTime();
        float64 DeltaTime = FrameStartTime - GlobalAppState.LastTime;
        kraft::Time::DeltaTime = DeltaTime;
        GlobalAppState.TimeSinceLastFrame += DeltaTime;
        GlobalAppState.LastTime = FrameStartTime;

        // Poll events
        kraft::Engine::Tick();

        Update(kraft::Time::DeltaTime);

        if (!kraft::Engine::Suspended)
        {
            Render();
            // kraft::Engine::Present();
            kraft::Engine::Renderer.PrepareFrame();
            kraft::Engine::Renderer.DrawSurfaces();

            // EditorState::Ptr->ObjectPickingRenderTarget.Begin();

            // kraft::GlobalUniformData GlobalShaderData = {};
            // kraft::renderer::Renderer->Camera = &EditorState::Ptr->CurrentWorld->Camera;
            // GlobalShaderData.Projection = EditorState::Ptr->CurrentWorld->Camera.ProjectionMatrix;
            // GlobalShaderData.View = EditorState::Ptr->CurrentWorld->Camera.GetViewMatrix();
            // GlobalShaderData.CameraPosition = EditorState::Ptr->CurrentWorld->Camera.Position;
            // kraft::ShaderSystem::ApplyGlobalProperties(GlobalShaderData);
            // kraft::ShaderSystem::BindByID(EditorState::Ptr->ObjectPickingShader->ID);

            // auto Group = EditorState::Ptr->CurrentWorld->GetRegistry().group<kraft::MeshComponent, kraft::TransformComponent>();
            // for (auto EntityHandle : Group)
            // {
            //     auto [Transform, Mesh] = Group.get<kraft::TransformComponent, kraft::MeshComponent>(EntityHandle);
            //     kraft::Mat4f ModelMatrix = EditorState::Ptr->CurrentWorld->GetWorldSpaceTransformMatrix(kraft::Entity(EntityHandle, EditorState::Ptr->CurrentWorld));
            //     kraft::ShaderSystem::SetUniform("Model", ModelMatrix);
            //     kraft::Engine::Renderer.DrawGeometry(Mesh.GeometryID);
            // }

            // kraft::ShaderSystem::ShaderSystem::Unbind();
            // EditorState::Ptr->ObjectPickingRenderTarget.End();

            // TODO (amn): Fix this
            // We do not have any other way to render the main renderpass right now :(
            if (kraft::Engine::Renderer.BeginMainRenderpass())
            {
                GlobalAppState.ImGuiRenderer.BeginFrame();
                GlobalAppState.ImGuiRenderer.RenderWidgets();
                GlobalAppState.ImGuiRenderer.EndFrame();

                if (!kraft::Engine::Renderer.EndMainRenderpass())
                {
                    KERROR("[RendererFrontend::DrawFrame]: End frame failed!");
                }
            }

            GlobalAppState.ImGuiRenderer.EndFrameUpdatePlatformWindows();
        }

        kraft::Time::FrameTime = kraft::Platform::GetAbsoluteTime() - FrameStartTime;
        if (kraft::Time::FrameTime < GlobalAppState.TargetFrameTime)
        {
            float64 CurrentAbsoluteTime = kraft::Platform::GetAbsoluteTime();
            // kraft::Platform::SleepMilliseconds((uint64)((GlobalAppState.TargetFrameTime - kraft::Time::FrameTime) * 1000.f));
        }

        if (GlobalAppState.TimeSinceLastFrame >= 1.f)
        {
            kraft::StringFormat(
                GlobalAppState.WindowTitleBuffer,
                sizeof(GlobalAppState.WindowTitleBuffer),
                "%s (%d fps | %f ms deltaTime | %f ms targetFrameTime | %.3f MiB Memory Usage)",
                kraft::Engine::Config.WindowTitle,
                GlobalAppState.FrameCount,
                DeltaTime * 1000.f,
                GlobalAppState.TargetFrameTime * 1000.0f,
                kraft::GetMemoryStats().AllocatedMiB
            );

            kraft::Platform::GetWindow()->SetWindowTitle(GlobalAppState.WindowTitleBuffer);

            GlobalAppState.TimeSinceLastFrame = 0.f;
            GlobalAppState.FrameCount = 0;
        }

        GlobalAppState.FrameCount++;
    }
}

#if defined(KRAFT_PLATFORM_WINDOWS) && !defined(KRAFT_DEBUG) && defined(KRAFT_GUI_APP)
#define KRAFT_USE_WINMAIN 1
#else
#define KRAFT_USE_WINMAIN 0
#endif

#if KRAFT_USE_WINMAIN
#include "Windows.h"
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char* argv[])
#endif
{
#if KRAFT_USE_WINMAIN
    int    argc = __argc;
    char** argv = __argv;
#endif

    kraft::Engine::Init(
        argc,
        argv,
        {
            .WindowWidth = 1280,
            .WindowHeight = 800,
            .WindowTitle = "Kraft Editor",
            .ApplicationName = "KraftEditor",
            .RendererBackend = kraft::renderer::RENDERER_BACKEND_TYPE_VULKAN,
            .ConsoleApp = false,
            .StartMaximized = true,
        }
    );

    Init();
    Run();
    Shutdown();

    kraft::Engine::Destroy();

    return 0;
}
