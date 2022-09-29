#include <stdio.h>

// #define GLAD_VULKAN_IMPLEMENTATION
// #include <glad/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "platform/kraft_platform.h"
#include "core/kraft_asserts.h"
#include "core/kraft_main.h"
#include "core/kraft_memory.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"

bool KeyDownEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int keycode = data.Int32[0];
    KINFO("%d key pressed", keycode);

    return false;
}

bool KeyUpEventListener(kraft::EventType type, void* sender, void* listener, kraft::EventData data) 
{
    int keycode = data.Int32[0];
    KINFO("%d key released", keycode);

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
    KINFO("%d mouse button pressed", button);

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

    return true;
}

void Update(float64 deltaTime)
{
    // KINFO("%f ms", kraft::Platform::GetElapsedTime());
}

void Render(float64 deltaTime)
{

}

void OnResize(size_t width, size_t height)
{

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

    app->Init                   = Init;
    app->Update                 = Update;
    app->Render                 = Render;
    app->OnResize               = OnResize;
    app->OnBackground           = OnBackground;
    app->OnForeground           = OnForeground;
    app->Shutdown               = Shutdown;

    return true;
}

