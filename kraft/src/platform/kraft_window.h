#pragma once

#include <core/kraft_core.h>
#include <platform/kraft_window_types.h>

#define KRAFT_ERROR_GLFW_INIT_FAILED          1
#define KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED 2

struct GLFWwindow;

namespace kraft::renderer {
enum RendererBackendType : int;
}

namespace kraft {

struct KRAFT_API Window
{
    GLFWwindow* PlatformWindowHandle;

    int        Init(const char* title, size_t width, size_t height, renderer::RendererBackendType backendType);
    bool       PollEvents(); // Returns false if the window wants to close
    void       SetWindowTitle(const char* title);
    void       Minimize();
    void       Maximize();
    void       SetCursorMode(CursorMode Mode);
    CursorMode GetCursorMode();
    void       SetCursorPosition(float64 X, float64 Y);
    void       GetCursorPosition(float64* X, float64* Y);
    void       Destroy();

    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowMaximizeCallback(GLFWwindow* window, int maximized);
    static void KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void CursorPositionCallback(GLFWwindow* window, double x, double y);
    static void DragDropCallback(GLFWwindow* window, int count, const char** paths);

    int     Width;
    int     Height;
    float32 DPI;
};

}