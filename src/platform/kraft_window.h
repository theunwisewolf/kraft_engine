#pragma once

#include "core/kraft_core.h"
#include "renderer/kraft_renderer_types.h"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>

#define KRAFT_ERROR_GLFW_INIT_FAILED 1
#define KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED 2

namespace kraft 
{

struct Window
{
    GLFWwindow* PlatformWindowHandle;

    int Init(const char* title, size_t width, size_t heightm, RendererBackendType backendType);
    bool PollEvents(); // Returns false if the window wants to close
    void Destroy();
    
    static void KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void CursorPositionCallback(GLFWwindow* window, double x, double y);
};

}