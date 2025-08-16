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

#include <kraft_types.h>

namespace kraft {

int Window::Init(const WindowOptions* Opts)
{
    MemSet(this, 0, sizeof(Window));

    if (!glfwInit())
    {
        KERROR("glfwInit() failed");
        return KRAFT_ERROR_GLFW_INIT_FAILED;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, Opts->StartMaximized);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);

    // TODO (amn): Handle window hints

    // Remove the title bar
    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    this->PlatformWindowHandle = glfwCreateWindow(Opts->Width, Opts->Height, *Opts->Title, NULL, NULL);
    if (!this->PlatformWindowHandle)
    {
        glfwTerminate();

        KERROR("glfwCreateWindow() failed");
        return KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED;
    }

    this->Width = Opts->Width;
    this->Height = Opts->Height;

    // We need to update the width and height if we start maximized
    if (Opts->StartMaximized)
    {
        glfwGetWindowSize(this->PlatformWindowHandle, &this->Width, &this->Height);
    }
    else
    {
// Center the window
#if defined(KRAFT_PLATFORM_WINDOWS)
        int maxWidth = GetSystemMetrics(SM_CXSCREEN);
        int maxHeight = GetSystemMetrics(SM_CYSCREEN);
        glfwSetWindowPos(this->PlatformWindowHandle, (maxWidth / 2) - (this->Width / 2), (maxHeight / 2) - (this->Height / 2));
#endif
    }

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

    uint32 LengthToCopy = StringLengthClamped(*Opts->Title, sizeof(this->Title));
    StringNCopy(this->Title, *Opts->Title, LengthToCopy);

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

    EventDataResize data;
    data.width = Width;
    data.height = Height;
    data.maximized = false;

    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_RESIZE, *(EventData*)(&data), Self);
}

void Window::WindowMaximizeCallback(GLFWwindow* window, int maximized)
{
    int Width, Height;
    glfwGetWindowSize(window, &Width, &Height);

    Window* Self = (Window*)glfwGetWindowUserPointer(window);
    Self->Width = Width;
    Self->Height = Height;

    EventDataResize data;
    data.width = Width;
    data.height = Height;
    data.maximized = true;

    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_MAXIMIZE, *(EventData*)(&data), Self);
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

    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_DRAG_DROP, data, glfwGetWindowUserPointer(window));
}

} // namespace kraft
