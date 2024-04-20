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

static char TextureName[] = "res/textures/test-vert-image-2.jpg";
static char TextureNameWide[] = "res/textures/test-wide-image-1.jpg";

bool OnDragDrop(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int count = (int)data.Int64[0];
    const char **paths = (const char**)data.Int64[1];

    // Try loading the texture from the command line args
    const char *TexturePath = paths[count - 1];
    if (kraft::filesystem::FileExists(TexturePath))
    {
        KINFO("Loading texture from path %s", TexturePath);
        kraft::MaterialSystem::SetTexture(TestSceneState->ObjectState.MaterialInstance, "DiffuseSampler", TexturePath);
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
    kraft::Keys KeyCode = (kraft::Keys)data.Int32[0];
    KINFO("%s key pressed (code = %d)", kraft::Platform::GetKeyName(KeyCode), KeyCode);

    static bool DefaultTexture = false;
    if (KeyCode == kraft::KEY_C)
    {
        kraft::String TexturePath = DefaultTexture ? TextureName : TextureNameWide;
        kraft::MaterialSystem::SetTexture(TestSceneState->ObjectState.MaterialInstance, "DiffuseSampler", TexturePath);
        DefaultTexture = !DefaultTexture;
        UpdateObjectScale();
    }

    return false;
}

bool KeyUpEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::Keys keycode = (kraft::Keys)data.Int32[0];
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
    int button = data.Int32[0];
    KINFO("%d mouse button pressed", button);

    return false;
}

bool MouseUpEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int button = data.Int32[0];
    KINFO("%d mouse button released", button);

    return false;
}

bool MouseDragStartEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int x = data.Int32[0];
    int y = data.Int32[1];
    // KINFO("Drag started = %d, %d", x, y);

    return false;
}

bool MouseDragDraggingEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::ApplicationConfig Config = kraft::Application::Get()->Config;
    float x = 2.0f * data.Int32[0] / (float)Config.WindowWidth - 1.0f;
    float y = 2.0f * data.Int32[1] / (float)Config.WindowHeight - 1.0f;

    kraft::Camera& Camera = TestSceneState->SceneCamera;
    kraft::Mat4f ViewProjectionInverse = kraft::Inverse(TestSceneState->ProjectionMatrix * Camera.ViewMatrix);
    kraft::Vec4f ScreenPointNDC = kraft::Vec4f(x, y, 0.0f, 1.0f);
    kraft::Vec4f WorldSpacePosition = ViewProjectionInverse * ScreenPointNDC;
    WorldSpacePosition /= WorldSpacePosition.w;

    TestSceneState->ObjectState.Position.x = WorldSpacePosition.x;
    TestSceneState->ObjectState.Position.y = -WorldSpacePosition.y;
    TestSceneState->ObjectState.ModelMatrix = kraft::ScaleMatrix(TestSceneState->ObjectState.Scale) * kraft::RotationMatrixFromEulerAngles(TestSceneState->ObjectState.Rotation) * kraft::TranslationMatrix(TestSceneState->ObjectState.Position);

#if 0
    float32 speed = 1.f;

    kraft::MousePosition MousePositionPrev = kraft::InputSystem::GetPreviousMousePosition();
    int x = data.Int32[0] - MousePositionPrev.x;
    int y = data.Int32[1] - MousePositionPrev.y;
    // int x = data.Int32[0];
    // int y = data.Int32[1];

    KINFO("Delta position (Screen) = %d, %d", x, y);

    // kraft::Vec3f direction = kraft::Normalize(kraft::Vec3f(-x, y, 0.f));
    // kraft::Vec4f direction = kraft::Vec4f(-x, y, 0.f, 0.f);
    kraft::Vec4f screenPoint = kraft::Vec4f(data.Int32[0], data.Int32[1], 0.0f, 0.0f);
    kraft::Vec4f screenPointNDC = kraft::Vec4f(data.Int32[0] / 1280.0f, data.Int32[1] / 800.0f, screenPoint.z, 0.0f) * 2.0f - 1.0f;

    // Screen to world
    kraft::Mat4f inverse = kraft::Inverse(TestSceneState->ProjectionMatrix * camera->ViewMatrix);
    // kraft::Vec3f ndc = kraft::Vec3f(screenPoint.x / 1280.0f, 1.0f - screenPoint.y / 800.f, screenPoint.z) * 2.0f - 1.0f;
    kraft::Vec4f worldPosition = (inverse * kraft::Vec4f(screenPointNDC.x, screenPointNDC.y, 0.0f, 1.0f));
    KINFO("World position homogenous = %f, %f", worldPosition.x, worldPosition.y);
    worldPosition = worldPosition / worldPosition.w;

    KINFO("World position = %f, %f", worldPosition.x, worldPosition.y);

    // kraft::Vec3f position = camera->Position;
    // kraft::Vec3f change = direction * speed;// * (float32)kraft::Time::DeltaTime;
    
    // camera->SetPosition(camera->Position + position);
    // camera->SetPosition(worldPosition.xyz);
#endif

    return false;
}

bool MouseDragEndEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int x = data.Int32[0];
    int y = data.Int32[1];
    // KINFO("Drag ended at = %d, %d", x, y);

    return false;
}

bool ScrollEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::Camera& Camera = TestSceneState->SceneCamera;

    float y = data.Float64[1];
    float32 speed = 50.f;

    if (TestSceneState->Projection == ProjectionType::Perspective)
    {
        kraft::Vec3f direction = kraft::ForwardVector(Camera.ViewMatrix);
        direction = direction * y;

        kraft::Vec3f position = Camera.Position;
        kraft::Vec3f change = direction * speed * (float32)kraft::Time::DeltaTime;
        Camera.SetPosition(position + change);
        Camera.Dirty = true;
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

        TestSceneState->ObjectState.Scale *= zoom;
        TestSceneState->ObjectState.ModelMatrix = kraft::ScaleMatrix(TestSceneState->ObjectState.Scale) * kraft::RotationMatrixFromEulerAngles(TestSceneState->ObjectState.Rotation) * kraft::TranslationMatrix(TestSceneState->ObjectState.Position);
    }

    return false;
}

bool Init()
{
    // KFATAL("Fatal error");
    // KERROR("Error");
    // KWARN("Warning");
    // KINFO("Info");
    // KSUCCESS("Success");

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

    Mat4f a(Identity), b(Identity);
    a._data[5] = 9;
    a._data[7] = 8;
    a._data[11] = 3;
    b._data[3] = 5;
    b._data[10] = 2;

    PrintMatrix(a);
    PrintMatrix(b);
    PrintMatrix(a * b);

    // kraft::renderer::ShaderEffect Output;
    // KASSERT(kraft::renderer::LoadShaderFX(Application::Get()->BasePath + "/res/shaders/basic.kfx.bkfx", Output));

    // RenderPipeline Pipeline = renderer::Renderer->CreateRenderPipeline(Output, 0);

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

    MaterialData MaterialConfig;
    MaterialConfig.Name = "simple_2d";
    MaterialConfig.DiffuseColor = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    MaterialConfig.AutoRelease = true;
    MaterialConfig.DiffuseTextureMapName = TexturePath;
    MaterialConfig.ShaderAsset = "res/shaders/basic.kfx.bkfx";

    TestSceneState->ObjectState.MaterialInstance = kraft::MaterialSystem::AcquireMaterialWithData(MaterialConfig);
    TestSceneState->ObjectState.MaterialInstance = kraft::MaterialSystem::AcquireMaterial("res/materials/simple_2d.kmt");

    // if (!TestSceneState.ObjectState.Texture)
    //     TestSceneState.ObjectState.Texture = TextureSystem::AcquireTexture(TextureName);
    // TestSceneState.ObjectState.Texture = TextureSystem::GetDefaultDiffuseTexture();

    SetProjection(ProjectionType::Orthographic);
    
    // To preserve the aspect ratio of the texture
    kraft::Texture* Texture = TestSceneState->ObjectState.MaterialInstance->DiffuseMap.Texture; 
    UpdateObjectScale();

    TestSceneState->ViewMatrix = TestSceneState->SceneCamera.GetViewMatrix();
    TestSceneState->SceneCamera.Dirty = true;
    TestSceneState->ObjectState.Dirty = true;

    InitImguiWidgets(Texture->Name);

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

    kraft::GeometryData GeometryData;
    GeometryData.Indices = indices;
    GeometryData.IndexSize = sizeof(uint32);
    GeometryData.IndexCount = sizeof(indices) / sizeof(indices[0]);
    GeometryData.Vertices = verts;
    GeometryData.VertexSize = sizeof(Vertex3D);
    GeometryData.VertexCount = 4;
    GeometryData.Name = "test-geometry";
    TestSceneState->ObjectState.GeometryID = kraft::GeometrySystem::AcquireGeometryWithData(GeometryData)->InternalID;
    TestSceneState->ObjectState.GeometryID = kraft::GeometrySystem::GetDefaultGeometry()->InternalID;

    TestSceneState->ViewMatrix = TestSceneState->SceneCamera.GetViewMatrix();

    return true;
}

void Update(float64 deltaTime)
{
    // KINFO("%f ms", kraft::Platform::GetElapsedTime());
    kraft::Camera *camera = &TestSceneState->SceneCamera;
    if (!kraft::InputSystem::IsMouseButtonDown(kraft::MouseButtons::MOUSE_BUTTON_RIGHT))
    {
        float32 speed = 50.f;
        kraft::Vec3f direction = kraft::Vec3fZero;
        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_UP) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_W))
        {
            if (TestSceneState->Projection == ProjectionType::Orthographic)
            {
                kraft::Vec3f upVector = UpVector(camera->GetViewMatrix());
                direction += upVector;
            }
            else
            {
                kraft::Vec3f forwardVector = ForwardVector(camera->GetViewMatrix());
                direction += forwardVector;
            }
        }
        else if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_DOWN) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_S))
        {
            if (TestSceneState->Projection == ProjectionType::Orthographic)
            {
                kraft::Vec3f downVector = DownVector(camera->GetViewMatrix());
                direction += downVector;
            }
            else
            {
                kraft::Vec3f backwardVector = BackwardVector(camera->GetViewMatrix());
                direction += backwardVector;
            }
        }

        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_LEFT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_A))
        {
            kraft::Vec3f leftVector = LeftVector(camera->GetViewMatrix());
            direction += leftVector;
        }
        else if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_RIGHT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_D))
        {
            kraft::Vec3f rightVector = RightVector(camera->GetViewMatrix());
            direction += rightVector;
        }

        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_SPACE))
        {
            if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_LEFT_SHIFT))
            {
                kraft::Vec3f down = DownVector(camera->GetViewMatrix());
                direction += down;
            }
            else
            {
                kraft::Vec3f up = UpVector(camera->GetViewMatrix());
                direction += up;
            }
        }

        if (!Vec3fCompare(direction, kraft::Vec3fZero, 0.00001f))
        {
            direction = kraft::Normalize(direction);
            kraft::Vec3f position = camera->Position;
            kraft::Vec3f change = direction * speed * (float32)deltaTime;
            camera->SetPosition(position + change);
        }
    }
    else
    {
        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_UP) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_W))
        {
            camera->SetPitch(camera->Pitch + 1.f * deltaTime);
        }

        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_DOWN) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_S))
        {
            camera->SetPitch(camera->Pitch - 1.f * deltaTime);
        }

        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_LEFT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_A))
        {
            camera->SetYaw(camera->Yaw + 1.f * deltaTime);
        }

        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_RIGHT) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_D))
        {
            camera->SetYaw(camera->Yaw - 1.f * deltaTime);
        }
    }

    if (camera->Dirty)
    {
        TestSceneState->ViewMatrix = TestSceneState->SceneCamera.GetViewMatrix();
    }
}

void Render(float64 deltaTime, kraft::renderer::RenderPacket& RenderPacket)
{
    RenderPacket.ProjectionMatrix = TestSceneState->ProjectionMatrix;
    RenderPacket.ViewMatrix = TestSceneState->ViewMatrix;

    kraft::Renderer->AddRenderable(TestSceneState->ObjectState);

    // kraft::Renderable Object2;
    // Object2.Geometry.Geometry = TestSceneState->ObjectState.Geometry;
    // Object2.Geometry.ModelMatrix = kraft::ScaleMatrix(TestSceneState->ObjectState.Scale / 3.0f) * kraft::TranslationMatrix(kraft::Vec3f(500.0f, 0, 0));
    // Object2.MaterialInstance = TestSceneState->ObjectState.Material;
    // kraft::Renderer->AddRenderable(Object2);
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

