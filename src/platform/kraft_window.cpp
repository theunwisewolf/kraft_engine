#include "kraft_window.h"

#include <GLFW/glfw3.h>

#include "core/kraft_core.h"
#include "core/kraft_events.h"
#include "core/kraft_input.h"
#include "core/kraft_log.h"

#if defined(KRAFT_PLATFORM_WINDOWS)
#include <Windows.h>
#endif

namespace kraft
{

int Window::Init(const char* title, size_t width, size_t height, RendererBackendType backendType)
{
    if (!glfwInit())
    {
        KERROR("glfwInit() failed");
        return KRAFT_ERROR_GLFW_INIT_FAILED;
    }

    if (backendType == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    else if (backendType == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {

    }

    this->PlatformWindowHandle = glfwCreateWindow(int(width), int(height), title, NULL, NULL);
    if (!this->PlatformWindowHandle)
    {
        glfwTerminate();

        KERROR("glfwCreateWindow() failed");
        return KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED;
    }

// Center the window
#if defined(KRAFT_PLATFORM_WINDOWS)
    int maxWidth = GetSystemMetrics(SM_CXSCREEN);
    int maxHeight = GetSystemMetrics(SM_CYSCREEN);
    glfwSetWindowPos(this->PlatformWindowHandle, (maxWidth / 2) - ((int)width / 2), (maxHeight / 2) - ((int)height / 2));
#endif

    // Window callbacks
    glfwSetWindowUserPointer(this->PlatformWindowHandle, this);
    glfwSetWindowSizeCallback(this->PlatformWindowHandle, WindowSizeCallback);
    glfwSetKeyCallback(this->PlatformWindowHandle, KeyCallback);
    glfwSetMouseButtonCallback(this->PlatformWindowHandle, MouseButtonCallback);
    glfwSetScrollCallback(this->PlatformWindowHandle, ScrollCallback);
    glfwSetCursorPosCallback(this->PlatformWindowHandle, CursorPositionCallback);

    return 0;
}

void Window::Destroy()
{
    glfwDestroyWindow(this->PlatformWindowHandle);
    glfwTerminate();
}

bool Window::PollEvents()
{
    glfwPollEvents();
    return !glfwWindowShouldClose(this->PlatformWindowHandle);
}

void Window::WindowSizeCallback(GLFWwindow* window, int width, int height)
{
    EventData data;
    data.UInt32[0] = width;
    data.UInt32[1] = height;

    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_RESIZE, data, glfwGetWindowUserPointer(window));
}

void Window::KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
    if (keycode == GLFW_KEY_UNKNOWN) return;

    bool pressed = action != GLFW_RELEASE;
    InputSystem::ProcessKeyboard(keycode, pressed);
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    InputSystem::ProcessMouseButton(button, action != GLFW_PRESS);
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{

}

void Window::CursorPositionCallback(GLFWwindow* window, double x, double y)
{
    InputSystem::ProcessMouseMove(int(x), int(y));
}

}
