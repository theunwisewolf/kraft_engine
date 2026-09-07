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

WindowEvent* WindowEventQueue::PushDrop(int path_count, const char** paths) {
    drop_path_count = 0;
    drop_path_storage_used = 0;
    for (int path_index = 0; path_index < path_count && drop_path_count < KRAFT_MAX_DROP_PATHS; path_index++) {
        u64 length = CStringLength((u8*)paths[path_index]);
        if (drop_path_storage_used + length + 1 > KRAFT_DROP_PATH_STORAGE_SIZE)
            break;

        u8* destination = drop_path_storage + drop_path_storage_used;
        MemCpy(destination, paths[path_index], length);
        destination[length] = 0;
        drop_paths[drop_path_count++] = String8{{destination}, length};
        drop_path_storage_used += (u32)(length + 1);
    }

    WindowEvent* event = Push(WINDOW_EVENT_DROP);
    if (event) {
        event->paths = drop_paths;
        event->path_count = drop_path_count;
    }

    return event;
}

int Window::Init(const WindowOptions* Opts) {
    MemSet(this, 0, sizeof(Window));

    if (!glfwInit()) {
        KERROR("glfwInit() failed");
        return KRAFT_ERROR_GLFW_INIT_FAILED;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, Opts->StartMaximized);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);

    this->PlatformWindowHandle = glfwCreateWindow(Opts->Width, Opts->Height, *Opts->Title, NULL, NULL);
    if (!this->PlatformWindowHandle) {
        glfwTerminate();
        KERROR("glfwCreateWindow() failed");
        return KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED;
    }

    glfwGetWindowSize(this->PlatformWindowHandle, &this->Width, &this->Height);
    glfwGetFramebufferSize(this->PlatformWindowHandle, &this->FramebufferWidth, &this->FramebufferHeight);

    this->DPI = 1.0f;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        f32 scale_x, scale_y;
        glfwGetMonitorContentScale(monitor, &scale_x, &scale_y);
        this->DPI = scale_x;

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode && !Opts->StartMaximized) {
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
    this->Events.Clear();
    this->Events.cursor_position = Vec2f((f32)cursor_x, (f32)cursor_y);
    InputSystem::SetMousePosition(cursor_x, cursor_y);

    u32 LengthToCopy = StringLengthClamped(*Opts->Title, sizeof(this->Title));
    StringNCopy(this->Title, *Opts->Title, LengthToCopy);

    return 0;
}

void Window::Destroy() {
    glfwDestroyWindow(this->PlatformWindowHandle);
    glfwTerminate();
}

bool Window::PollEvents() {
    this->Events.Clear();
    glfwPollEvents();
    InputSystem::ProcessEvents(&this->Events);

    return !this->ShouldClose();
}

bool Window::WaitEvents(f64 timeout_seconds) {
    this->Events.Clear();
    glfwWaitEventsTimeout(timeout_seconds);
    InputSystem::ProcessEvents(&this->Events);

    return !this->ShouldClose();
}

bool Window::ShouldClose() {
    return glfwWindowShouldClose(this->PlatformWindowHandle) != 0;
}

bool Window::IsMaximized() {
    return glfwGetWindowAttrib(this->PlatformWindowHandle, GLFW_MAXIMIZED) != 0;
}

void Window::SetWindowTitle(const char* title) {
    glfwSetWindowTitle(this->PlatformWindowHandle, title);
}

void Window::Minimize() {
    glfwIconifyWindow(this->PlatformWindowHandle);
}

void Window::Maximize() {
    glfwMaximizeWindow(this->PlatformWindowHandle);
}

void Window::SetCursorMode(CursorMode Mode) {
    glfwSetInputMode(this->PlatformWindowHandle, GLFW_CURSOR, (int)Mode);
}

CursorMode Window::GetCursorMode() {
    return CursorMode(glfwGetInputMode(this->PlatformWindowHandle, GLFW_CURSOR));
}

void Window::SetCursorPosition(f64 X, f64 Y) {
    glfwSetCursorPos(this->PlatformWindowHandle, (double)X, (double)Y);
}

void Window::GetCursorPosition(f64* X, f64* Y) {
    glfwGetCursorPos(this->PlatformWindowHandle, X, Y);
}

String8 Window::ClipboardGet(ArenaAllocator* arena) {
    const char* text = glfwGetClipboardString(this->PlatformWindowHandle);
    if (!text)
        return String8{};

    return StringCopy(arena, String8FromCString(text));
}

void Window::ClipboardSet(String8 text) {
    TempArena scratch = ScratchBegin(0, 0);
    char* terminated = ArenaPushString(scratch.arena, text.str, text.count);
    glfwSetClipboardString(this->PlatformWindowHandle, terminated);
    ScratchEnd(scratch);
}

i32 Window::CreateVulkanSurface(VkInstance_T* instance, VkSurfaceKHR_T** out_surface) {
    return (i32)glfwCreateWindowSurface((VkInstance)instance, this->PlatformWindowHandle, nullptr, (VkSurfaceKHR*)out_surface);
}

const char** Window::RequiredVulkanExtensions(u32* count) {
    return glfwGetRequiredInstanceExtensions(count);
}

void Window::WindowSizeCallback(GLFWwindow* window, int width, int height) {
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Width = width;
    self->Height = height;

    WindowEvent* event = self->Events.Push(WINDOW_EVENT_RESIZE);
    if (event) {
        event->width = width;
        event->height = height;
    }

    EventDataResize data;
    data.width = width;
    data.height = height;
    data.maximized = false;
    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_RESIZE, *(EventData*)(&data), self);
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->FramebufferWidth = width;
    self->FramebufferHeight = height;
    self->FramebufferResized = true;

    WindowEvent* event = self->Events.Push(WINDOW_EVENT_FRAMEBUFFER_RESIZE);
    if (event) {
        event->width = width;
        event->height = height;
    }
}

void Window::WindowMaximizeCallback(GLFWwindow* window, int maximized) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Width = width;
    self->Height = height;

    WindowEvent* event = self->Events.Push(WINDOW_EVENT_MAXIMIZE);
    if (event) {
        event->width = width;
        event->height = height;
        event->maximized = maximized != 0;
    }

    EventDataResize data;
    data.width = width;
    data.height = height;
    data.maximized = maximized != 0;
    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_MAXIMIZE, *(EventData*)(&data), self);
}

void Window::KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods) {
    if (keycode == GLFW_KEY_UNKNOWN)
        return;

    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Events.modifiers = (u32)mods;

    WindowEvent* event = self->Events.Push(action != GLFW_RELEASE ? WINDOW_EVENT_KEY_PRESS : WINDOW_EVENT_KEY_RELEASE);
    if (event) {
        event->key = (Keys)keycode;
        event->repeat = action == GLFW_REPEAT;
    }
}

void Window::CharCallback(GLFWwindow* window, unsigned int codepoint) {
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    WindowEvent* event = self->Events.Push(WINDOW_EVENT_TEXT);
    if (event)
        event->codepoint = codepoint;
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Events.modifiers = (u32)mods;

    WindowEvent* event = self->Events.Push(action == GLFW_PRESS ? WINDOW_EVENT_MOUSE_PRESS : WINDOW_EVENT_MOUSE_RELEASE);
    if (event)
        event->button = (MouseButtons)button;
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    WindowEvent* event = self->Events.Push(WINDOW_EVENT_SCROLL);
    if (event)
        event->scroll = Vec2f((f32)xoffset, (f32)yoffset);
}

void Window::CursorPositionCallback(GLFWwindow* window, double x, double y) {
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Events.cursor_position = Vec2f((f32)x, (f32)y);
    self->Events.Push(WINDOW_EVENT_MOUSE_MOVE);
}

void Window::DragDropCallback(GLFWwindow* window, int count, const char** paths) {
    Window* self = (Window*)glfwGetWindowUserPointer(window);
    self->Events.PushDrop(count, paths);

    EventData data;
    data.Int64Value[0] = count;
    data.Int64Value[1] = (i64)paths;
    EventSystem::Dispatch(EventType::EVENT_TYPE_WINDOW_DRAG_DROP, data, self);
}

} // namespace kraft
