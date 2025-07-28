
#pragma once

#include "containers/kraft_array.h"
#include "core/kraft_string.h"

#include <imgui/imgui.h>

namespace kraft {

struct Texture;
struct TextureSampler;
namespace renderer {

template<typename>
struct Handle;

}
}

// "refresh" is true whenever the window has been resized
typedef void (*ImGuiRenderCallback)(bool refresh);

struct ImGuiWidget
{
    kraft::String       Name;
    ImGuiRenderCallback Callback = nullptr;

    ImGuiWidget(const kraft::String& Name, ImGuiRenderCallback Callback) : Name(Name), Callback(Callback)
    {}

    ImGuiWidget& operator=(const ImGuiWidget& Widget)
    {
        Name = Widget.Name;
        Callback = Widget.Callback;

        return *this;
    }
};

struct RendererImGui
{
    kraft::String             IniFilename;
    kraft::Array<ImGuiWidget> Widgets;

    bool        Init();
    void        AddWidget(const kraft::String& Name, ImGuiRenderCallback Callback);
    ImTextureID AddTexture(kraft::renderer::Handle<kraft::Texture> texture, kraft::renderer::Handle<kraft::TextureSampler> sampler);
    void        RemoveTexture(ImTextureID Texture);
    void        OnResize(int Width, int Height);
    void        RenderWidgets();
    void        Destroy();
    void        EndFrameUpdatePlatformWindows();

    void BeginFrame();
    void EndFrame();
};
