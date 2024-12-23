#include "kraft_window.h"

#include <GLFW/glfw3.h>

#include <containers/kraft_array.h>
#include <core/kraft_core.h>
#include <core/kraft_events.h>
#include <core/kraft_input.h>
#include <core/kraft_log.h>
#include <core/kraft_string.h>
#include <renderer/kraft_renderer_types.h>

#if defined(KRAFT_PLATFORM_WINDOWS)
#include <Windows.h>
#endif

namespace kraft {

int Window::Init(const char* title, size_t width, size_t height, renderer::RendererBackendType backendType)
{
    if (!glfwInit())
    {
        KERROR("glfwInit() failed");
        return KRAFT_ERROR_GLFW_INIT_FAILED;
    }

    if (backendType == renderer::RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    else if (backendType == renderer::RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {
    }

    // Remove the title bar
    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

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

    // TODO (amn): We should fetch the monitor the window is being drawn on
    // Currently there is no inbuilt way to do this in glfw
    // A possible workaround is mentioned here:
    // https://github.com/glfw/glfw/issues/1699
    // For now, we just use the primary monitor
    GLFWmonitor* CurrentMonitor = glfwGetPrimaryMonitor();
    float        XScale, YScale;
    glfwGetMonitorContentScale(CurrentMonitor, &XScale, &YScale);
    this->DPI = XScale;

    // Window callbacks
    glfwSetWindowUserPointer(this->PlatformWindowHandle, this);
    glfwSetWindowSizeCallback(this->PlatformWindowHandle, WindowSizeCallback);
    glfwSetWindowMaximizeCallback(this->PlatformWindowHandle, WindowMaximizeCallback);
    glfwSetKeyCallback(this->PlatformWindowHandle, KeyCallback);
    glfwSetMouseButtonCallback(this->PlatformWindowHandle, MouseButtonCallback);
    glfwSetScrollCallback(this->PlatformWindowHandle, ScrollCallback);
    glfwSetCursorPosCallback(this->PlatformWindowHandle, CursorPositionCallback);
    glfwSetDropCallback(this->PlatformWindowHandle, DragDropCallback);

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

void Window::SetWindowTitle(const char* title)
{
    glfwSetWindowTitle(this->PlatformWindowHandle, title);
}

void Window::Minimize()
{
    glfwHideWindow(this->PlatformWindowHandle);
}

void Window::Maximize()
{
    glfwMaximizeWindow(this->PlatformWindowHandle);
}

void Window::SetCursorMode(CursorMode Mode)
{
    glfwSetInputMode(this->PlatformWindowHandle, GLFW_CURSOR, (int)Mode);
}

CursorMode Window::GetCursorMode()
{
    return CursorMode(glfwGetInputMode(this->PlatformWindowHandle, GLFW_CURSOR));
}

void Window::SetCursorPosition(float64 X, float64 Y)
{
    glfwSetCursorPos(this->PlatformWindowHandle, (double)X, (double)Y);
}

void Window::GetCursorPosition(float64* X, float64* Y)
{
    glfwGetCursorPos(this->PlatformWindowHandle, X, Y);
}

void Window::WindowSizeCallback(GLFWwindow* window, int Width, int Height)
{
    Window* Self = (Window*)glfwGetWindowUserPointer(window);
    Self->Width = Width;
    Self->Height = Height;

    EventData data;
    data.UInt32Value[0] = Width;
    data.UInt32Value[1] = Height;
    data.UInt32Value[2] = 0; // Maximized

    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_RESIZE, data, Self);
}

void Window::WindowMaximizeCallback(GLFWwindow* window, int maximized)
{
    int Width, Height;
    glfwGetWindowSize(window, &Width, &Height);

    Window* Self = (Window*)glfwGetWindowUserPointer(window);
    Self->Width = Width;
    Self->Height = Height;

    EventData data;
    data.UInt32Value[0] = Width;
    data.UInt32Value[1] = Height;
    data.UInt32Value[2] = maximized;

    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_MAXIMIZE, data, Self);
}

void Window::KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
    if (keycode == GLFW_KEY_UNKNOWN)
        return;

    bool pressed = action != GLFW_RELEASE;
    InputSystem::ProcessKeyboard(keycode, pressed);
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    InputSystem::ProcessMouseButton(button, action == GLFW_PRESS);
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    InputSystem::ProcessScroll(xoffset, yoffset);
}

void Window::CursorPositionCallback(GLFWwindow* window, double x, double y)
{
    InputSystem::ProcessMouseMove(int(x), int(y));
}

void Window::DragDropCallback(GLFWwindow* window, int count, const char** paths)
{
    EventData data;
    data.Int64Value[0] = count;
    data.Int64Value[1] = (int64)paths;

    for (int i = 0; i < count; i++)
    {
        KDEBUG("Dropped file %s", ((char**)data.Int64Value[1])[i]);
    }

    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_DRAG_DROP, data, glfwGetWindowUserPointer(window));
}

}
