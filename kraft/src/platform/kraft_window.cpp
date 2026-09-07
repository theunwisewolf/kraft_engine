#include "kraft_window.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <core/kraft_allocators.h>
#include <core/kraft_core.h>
#include <core/kraft_events.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>
#include <core/kraft_strings.h>
#include <core/kraft_thread_context.h>
#include <platform/kraft_input.h>

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

    this->PlatformWindowHandle = glfwCreateWindow(Opts->Width, Opts->Height, *Opts->Title, NULL, NULL);
    if (!this->PlatformWindowHandle)
    {
        glfwTerminate();
        KERROR("glfwCreateWindow() failed");
        return KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED;
    }

    glfwGetWindowSize(this->PlatformWindowHandle, &this->Width, &this->Height);
    glfwGetFramebufferSize(this->PlatformWindowHandle, &this->FramebufferWidth, &this->FramebufferHeight);

    this->DPI = 1.0f;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor)
    {
        f32 scale_x, scale_y;
        glfwGetMonitorContentScale(monitor, &scale_x, &scale_y);
        this->DPI = scale_x;

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode && !Opts->StartMaximized)
        {
            glfwSetWindowPos(this->PlatformWindowHandle, (mode->width - this->Width) / 2, (mode->height - this->Height) / 2);
        }
    }

    glfwSetWindowUserPointer(this->PlatformWindowHandle, this);
    glfwSetWindowSizeCallback(this->PlatformWindowHandle, WindowSizeCallback);
    glfwSetFramebufferSizeCallback(this->PlatformWindowHandle, FramebufferSizeCallback);
    glfwSetWindowMaximizeCallback(this->PlatformWindowHandle, WindowMaximizeCallback);
    glfwSetKeyCallback(this->PlatformWindowHandle, KeyCallback);
    glfwSetCharCallback(this->PlatformWindowHandle, CharCallback);
    glfwSetMouseButtonCallback(this->PlatformWindowHandle, MouseButtonCallback);
    glfwSetScrollCallback(this->PlatformWindowHandle, ScrollCallback);
    glfwSetCursorPosCallback(this->PlatformWindowHandle, CursorPositionCallback);
    glfwSetDropCallback(this->PlatformWindowHandle, DragDropCallback);

    f64 cursor_x, cursor_y;
    glfwGetCursorPos(this->PlatformWindowHandle, &cursor_x, &cursor_y);
    InputSystem::SetMousePosition(cursor_x, cursor_y);

    u32 LengthToCopy = StringLengthClamped(*Opts->Title, sizeof(this->Title));
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
    return !this->ShouldClose();
}

bool Window::WaitEvents(f64 timeout_seconds)
{
    glfwWaitEventsTimeout(timeout_seconds);
    return !this->ShouldClose();
}

bool Window::ShouldClose()
{
    return glfwWindowShouldClose(this->PlatformWindowHandle) != 0;
}

bool Window::IsMaximized()
{
    return glfwGetWindowAttrib(this->PlatformWindowHandle, GLFW_MAXIMIZED) != 0;
}

void Window::SetWindowTitle(const char* title)
{
    glfwSetWindowTitle(this->PlatformWindowHandle, title);
}

void Window::Minimize()
{
    glfwIconifyWindow(this->PlatformWindowHandle);
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

void Window::SetCursorPosition(f64 X, f64 Y)
{
    glfwSetCursorPos(this->PlatformWindowHandle, (double)X, (double)Y);
}

void Window::GetCursorPosition(f64* X, f64* Y)
{
    glfwGetCursorPos(this->PlatformWindowHandle, X, Y);
}

String8 Window::ClipboardGet(ArenaAllocator* arena)
{
    const char* text = glfwGetClipboardString(this->PlatformWindowHandle);
    if (!text)
        return String8{};

    return StringCopy(arena, String8FromCString(text));
}

void Window::ClipboardSet(String8 text)
{
    TempArena scratch = ScratchBegin(0, 0);
    char* terminated = ArenaPushString(scratch.arena, text.str, text.count);
    glfwSetClipboardString(this->PlatformWindowHandle, terminated);
    ScratchEnd(scratch);
}

i32 Window::CreateVulkanSurface(VkInstance_T* instance, VkSurfaceKHR_T** out_surface)
{
    return (i32)glfwCreateWindowSurface((VkInstance)instance, this->PlatformWindowHandle, nullptr, (VkSurfaceKHR*)out_surface);
}

const char** Window::RequiredVulkanExtensions(u32* count)
{
    return glfwGetRequiredInstanceExtensions(count);
}

void Window::WindowSizeCallback(GLFWwindow* window, int width, int height)
{
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Width = width;
    self->Height = height;
    InputSystem::ProcessWindowResize(width, height, false);

    EventDataResize data;
    data.width = width;
    data.height = height;
    data.maximized = false;
    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_RESIZE, *(EventData*)(&data), self);
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->FramebufferWidth = width;
    self->FramebufferHeight = height;
    self->FramebufferResized = true;
    InputSystem::ProcessFramebufferResize(width, height);
}

void Window::WindowMaximizeCallback(GLFWwindow* window, int maximized)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Width = width;
    self->Height = height;
    InputSystem::ProcessWindowResize(width, height, maximized != 0);

    EventDataResize data;
    data.width = width;
    data.height = height;
    data.maximized = maximized != 0;
    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_MAXIMIZE, *(EventData*)(&data), self);
}

void Window::KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
    if (keycode == GLFW_KEY_UNKNOWN)
        return;

    InputSystem::ProcessKey(keycode, action != GLFW_RELEASE, action == GLFW_REPEAT, (u32)mods);
}

void Window::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    InputSystem::ProcessText(codepoint);
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    InputSystem::ProcessMouseButton(button, action == GLFW_PRESS, (u32)mods);
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    InputSystem::ProcessScroll(xoffset, yoffset);
}

void Window::CursorPositionCallback(GLFWwindow* window, double x, double y)
{
    InputSystem::ProcessMouseMove(x, y);
}

void Window::DragDropCallback(GLFWwindow* window, int count, const char** paths)
{
    InputSystem::ProcessDrop(count, paths);

    EventData data;
    data.Int64Value[0] = count;
    data.Int64Value[1] = (i64)paths;
    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_DRAG_DROP, data, glfwGetWindowUserPointer(window));
}

} // namespace kraft
