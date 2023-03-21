#include <stdio.h>

// #define GLAD_VULKAN_IMPLEMENTATION
// #include <glad/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "core/kraft_asserts.h"
#include "core/kraft_main.h"
#include "core/kraft_memory.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_string.h"
#include "math/kraft_math.h"
#include "platform/kraft_platform.h"
#include "platform/kraft_filesystem.h"
#include "renderer/kraft_renderer_types.h"
#include "core/kraft_string.h"

#include "renderer/vulkan/tests/simple_scene.h"

#include <imgui.h>

#include "scenes/test.cpp"

#include "networking/kraft_networking_types.h"
#include "networking/kraft_client.h"
#include "networking/kraft_server.h"

static kraft::Server s_Server;
static kraft::Client s_Client;

bool KeyDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    kraft::Keys keycode = (kraft::Keys)data.Int32[0];
    KINFO("%s key pressed (code = %d)", kraft::Platform::GetKeyName(keycode), keycode);

    if (keycode == kraft::KEY_ENTER)
    {
        char data[256];
        switch (rand() % 4)
        {
        case 0:
            kraft::StringCopy(data, "Hello world!");
            s_Client.Send(data, strlen(data));
            break;

        case 1:
            kraft::StringCopy(data, "Ahoy");
            s_Client.Send(data, strlen(data));
            break;

        case 2:
            kraft::StringCopy(data, "What's up?");
            s_Client.Send(data, strlen(data));
            break;

        case 3:
            kraft::StringCopy(data, "Am I random?");
            s_Client.Send(data, strlen(data));
            break;
        }
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

    Mat4f a(Identity), b(Identity);
    a._data[5] = 9;
    a._data[7] = 8;
    a._data[11] = 3;
    b._data[3] = 5;
    b._data[10] = 2;

    PrintMatrix(a);
    PrintMatrix(b);
    PrintMatrix(a * b);

    kraft::FileHandle handle;
    uint8* buffer = 0;
    kraft::filesystem::OpenFile("res/shaders/combined.shader", kraft::FILE_OPEN_MODE_READ, true, &handle);
    buffer = (uint8*)kraft::Malloc(kraft::filesystem::GetFileSize(&handle) + 1, kraft::MemoryTag::MEMORY_TAG_FILE_BUF, true);
    kraft::filesystem::ReadAllBytes(&handle, &buffer);
    kraft::filesystem::CloseFile(&handle);

    // printf("%s\n", buffer);
    kraft::Free(buffer);

    kraft::Socket::Init();
    kraft::SocketAddress address(0, 4000);
    kraft::SocketAddress serverAddress(127, 0, 0, 1, 4000);
    s_Server = kraft::Server(address, kraft::SERVER_UPDATE_MODE_MANUAL);
    s_Client.Init(serverAddress);
    KASSERT(s_Server.Start());

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
                kraft::Vec3f up = UpVector(camera->GetViewMatrix());
                direction += up;
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
                kraft::Vec3f down = DownVector(camera->GetViewMatrix());
                direction += down;
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

    s_Server.Update();
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


bool CreateApplication(kraft::Application* app)
{
    app->Config.ApplicationName = "Kraft!";
    app->Config.WindowTitle     = "Kraft! [VULKAN]";
    app->Config.WindowWidth     = 1024;
    app->Config.WindowHeight    = 768;
    app->Config.RendererBackend = kraft::RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN;

    app->Init                   = Init;
    app->Update                 = Update;
    app->Render                 = Render;
    app->OnResize               = OnResize;
    app->OnBackground           = OnBackground;
    app->OnForeground           = OnForeground;
    app->Shutdown               = Shutdown;

    return true;
}

