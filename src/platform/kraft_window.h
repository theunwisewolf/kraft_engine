#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_renderer_types.h"

#define KRAFT_ERROR_GLFW_INIT_FAILED 1
#define KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED 2

#if defined(KRAFT_PLATFORM_WINDOWS) || defined(KRAFT_PLATFORM_MACOS) || defined(KRAFT_PLATFORM_LINUX)
struct GLFWwindow;
#endif

namespace kraft 
{

struct KRAFT_API Window
{
#if defined(KRAFT_PLATFORM_WINDOWS) || defined(KRAFT_PLATFORM_MACOS) || defined(KRAFT_PLATFORM_LINUX)
    GLFWwindow* PlatformWindowHandle;
#endif

    int Init(const char* title, size_t width, size_t heightm, RendererBackendType backendType);
    bool PollEvents(); // Returns false if the window wants to close
    void Destroy();
    
    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void CursorPositionCallback(GLFWwindow* window, double x, double y);
};

}