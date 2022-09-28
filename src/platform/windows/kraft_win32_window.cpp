#include "kraft_win32_window.h"
#include "core/kraft_log.h"
#include "core/kraft_input.h"
#include "renderer/kraft_renderer_frontend.h"

#include <vulkan/vulkan.h>

namespace kraft
{

int Window::Init(const char* title, size_t width, size_t height)
{
    if (!glfwInit())
    {
        KERROR("KRAFT_ERROR_GLFW_INIT_FAILED");
        return KRAFT_ERROR_GLFW_INIT_FAILED;
    }

    Renderer.Type = RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN; // TODO: Configurable
    RendererFrontend::I = &Renderer;

    if (Renderer.Type == RendererBackendType::RENDERER_BACKEND_TYPE_VULKAN)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    else if (Renderer.Type == RendererBackendType::RENDERER_BACKEND_TYPE_OPENGL)
    {

    }

    if (!Renderer.Init())
    {
        KERROR("[Application::Create]: Failed to initalize renderer!");
        return false;
    }

    this->Window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!this->Window)
    {
        glfwTerminate();

        KERROR("KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED");
        return KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED;
    }

    // Window callbacks
    glfwSetKeyCallback(this->Window, KeyCallback);
    glfwSetMouseButtonCallback(this->Window, MouseButtonCallback);
    glfwSetScrollCallback(this->Window, ScrollCallback);
    glfwSetCursorPosCallback(this->Window, CursorPositionCallback);

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    KRAFT_LOG_INFO("Vulkan extensions count %d", extensionCount);
    return 0;
}

void Window::Destroy()
{
    Renderer.Shutdown();

    glfwDestroyWindow(Window);
    glfwTerminate();
}

bool Window::PollEvents()
{
    glfwPollEvents();
    return !glfwWindowShouldClose(this->Window);
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
    InputSystem::ProcessMouseMove(x, y);
}

}
