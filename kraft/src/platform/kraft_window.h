#pragma once

#include <core/kraft_core.h>
#include <core/kraft_math.h>
#include <platform/kraft_window_events.h>
#include <platform/kraft_window_types.h>

#define KRAFT_ERROR_GLFW_INIT_FAILED 1
#define KRAFT_ERROR_GLFW_CREATE_WINDOW_FAILED 2
#define KRAFT_MAX_CAPTION_EXCLUSIONS 8

struct GLFWwindow;
struct GLFWcursor;
struct VkInstance_T;
struct VkSurfaceKHR_T;

namespace kraft {

struct ArenaAllocator;
struct PlatformWindowState;

struct KRAFT_API Window {
    GLFWwindow* PlatformWindowHandle = nullptr;

    // Platform-specific window state
    PlatformWindowState* PlatformWindowState = nullptr;

    // Everything reported since the last poll, in order
    WindowEventQueue Events;

    int Init(const struct WindowOptions* Opts);
    bool PollEvents(); // Returns false if the window wants to close
    bool WaitEvents(f64 timeout_seconds); // Blocks until an event arrives or the timeout elapses
    bool ShouldClose();
    bool IsMaximized();
    void SetWindowTitle(const char* title);
    void SetIcon(i32 width, i32 height, const u8* rgba_pixels);
    void Minimize();
    void Maximize();
    void Restore();
    void Close();
    void SetCursorMode(CursorMode Mode);
    void SetCursorShape(CursorShape shape);
    CursorMode GetCursorMode();
    void SetCursorPosition(f64 X, f64 Y);
    void GetCursorPosition(f64* X, f64* Y);
    String8 ClipboardGet(ArenaAllocator* arena);
    void ClipboardSet(String8 text);
    i32 CreateVulkanSurface(VkInstance_T* instance, VkSurfaceKHR_T** out_surface);
    void Destroy();

    // For a custom title bar
    void SetCaptionRegion(f32 height, const Rect* excluded_rects, u32 excluded_count);

    static const char** RequiredVulkanExtensions(u32* count);

    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowMaximizeCallback(GLFWwindow* window, int maximized);
    static void KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
    static void CharCallback(GLFWwindow* window, unsigned int codepoint);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void CursorPositionCallback(GLFWwindow* window, double x, double y);
    static void DragDropCallback(GLFWwindow* window, int count, const char** paths);

    int Width;
    int Height;
    int FramebufferWidth;
    int FramebufferHeight;
    bool FramebufferResized;
    f32 DPI;
    char Title[1024] = {0};

    bool CustomTitleBar;
    f32 CaptionHeight;
    Rect CaptionExclusions[KRAFT_MAX_CAPTION_EXCLUSIONS];
    u32 CaptionExclusionCount;

    CursorShape CurrentCursorShape;
    GLFWcursor* Cursors[CURSOR_SHAPE_COUNT];
};

} // namespace kraft
