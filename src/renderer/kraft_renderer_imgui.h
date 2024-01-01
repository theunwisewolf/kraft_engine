#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "containers/kraft_array.h"

namespace kraft
{

struct ApplicationConfig;

// "refresh" is true whenever the window has been resized
typedef void (*ImGuiRenderCallback)(bool refresh);

struct ImGuiWidget
{
    String             Name;
    ImGuiRenderCallback Callback  = nullptr;

    ImGuiWidget(const String& Name, ImGuiRenderCallback Callback) :
        Name(Name),
        Callback(Callback)
    {
    }

    ImGuiWidget& operator=(const kraft::ImGuiWidget &Widget)
    {
        Name = Widget.Name;
        Callback = Widget.Callback;

        return *this;
    }
};

bool RendererCreateImguiBackend(RendererBackendType type, RendererImguiBackend* backend);

struct RendererImGui
{
    Block                   BackendMemory;
    RendererImguiBackend*   Backend = nullptr;

    String IniFilename;
    Array<ImGuiWidget> Widgets;

    bool Init(ApplicationConfig* config);
    void AddWidget(const String& Name, ImGuiRenderCallback Callback);
    void* AddTexture(Texture* Texture);
    void OnResize(int Width, int Height);
    void RenderWidgets();
    void Destroy();
    void EndFrameUpdatePlatformWindows();
};

}