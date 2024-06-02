#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "containers/kraft_array.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft
{

struct ApplicationConfig;

namespace renderer
{

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

    ImGuiWidget& operator=(const ImGuiWidget &Widget)
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

    String IniFilename;
    Array<ImGuiWidget> Widgets;

    bool Init(ApplicationConfig* config);
    void AddWidget(const String& Name, ImGuiRenderCallback Callback);
    ImTextureID AddTexture(Texture* Texture);
    void RemoveTexture(ImTextureID Texture);
    void OnResize(int Width, int Height);
    void RenderWidgets();
    void Destroy();
    void EndFrameUpdatePlatformWindows();

    void BeginFrame(float64 DeltaTime);
    void EndFrame();
};

}

}