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

#include "renderer/vulkan/tests/simple_scene.h"
#include "renderer/shaderfx/kraft_shaderfx.h"

#include <imgui.h>

#include "scenes/test.cpp"

bool KeyDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::Keys keycode = (kraft::Keys)data.Int32[0];
    KINFO("%s key pressed (code = %d)", kraft::Platform::GetKeyName(keycode), keycode);

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
    kraft::SceneState* sceneState = kraft::GetSceneState();
    kraft::Camera *camera = &sceneState->SceneCamera;

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
    kraft::Mat4f inverse = kraft::Inverse(sceneState->GlobalUBO.Projection * camera->ViewMatrix);
    // kraft::Vec3f ndc = kraft::Vec3f(screenPoint.x / 1280.0f, 1.0f - screenPoint.y / 800.f, screenPoint.z) * 2.0f - 1.0f;
    kraft::Vec4f worldPosition = (inverse * kraft::Vec4f(screenPointNDC.x, screenPointNDC.y, 0.0f, 1.0f));
    KINFO("World position homogenous = %f, %f", worldPosition.x, worldPosition.y);
    worldPosition = worldPosition / worldPosition.w;

    KINFO("World position = %f, %f", worldPosition.x, worldPosition.y);

    // kraft::Vec3f position = camera->Position;
    // kraft::Vec3f change = direction * speed;// * (float32)kraft::Time::DeltaTime;
    
    // camera->SetPosition(camera->Position + position);
    // camera->SetPosition(worldPosition.xyz);

    return false;
}

bool MouseDragEndEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int x = data.Int32[0];
    int y = data.Int32[1];
    // KINFO("Drag ended at = %d, %d", x, y);

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

    kraft::renderer::ShaderEffect Output;
    KASSERT(kraft::renderer::LoadShaderFX(Application::Get()->BasePath + "/res/shaders/basic.kfx.bkfx", Output));

    return true;
}

void Update(float64 deltaTime)
{
    // KINFO("%f ms", kraft::Platform::GetElapsedTime());
    kraft::SceneState* sceneState = kraft::GetSceneState();
    kraft::Camera *camera = &sceneState->SceneCamera;
    if (!kraft::InputSystem::IsMouseButtonDown(kraft::MouseButtons::MOUSE_BUTTON_RIGHT))
    {
        float32 speed = 50.f;
        kraft::Vec3f direction = kraft::Vec3fZero;
        if (kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_UP) || kraft::InputSystem::IsKeyDown(kraft::Keys::KEY_W))
        {
            if (sceneState->Projection == kraft::SceneState::ProjectionType::Orthographic)
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
            if (sceneState->Projection == kraft::SceneState::ProjectionType::Orthographic)
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
}

void Render(float64 deltaTime)
{

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
    return true;
}


bool CreateApplication(kraft::Application* app, int argc, char *argv[])
{
    app->Config.ApplicationName = "Kraft!";
    app->Config.WindowTitle     = "Kraft! [VULKAN]";
    app->Config.WindowWidth     = 1280;
    app->Config.WindowHeight    = 800;
    app->Config.RendererBackend = kraft::RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN;
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

