
#pragma once

#include <imgui/imgui.h>

namespace kraft {

struct Texture;
struct TextureSampler;
namespace r {

template<typename>
struct Handle;

}
} // namespace kraft

// "refresh" is true whenever the window has been resized
typedef void (*ImGuiRenderCallback)(bool refresh);

struct ImGuiWidget
{
    String8             name;
    ImGuiRenderCallback callback = nullptr;

    ImGuiWidget(String8 name, ImGuiRenderCallback callback) : name(name), callback(callback)
    {}

    ImGuiWidget& operator=(const ImGuiWidget& widget)
    {
        name = widget.name;
        callback = widget.callback;

        return *this;
    }
};

struct RendererImGui
{
    String8      ini_filename;
    ImGuiWidget* widgets;
    u64          widget_count = 0;

    bool        Init(ArenaAllocator* arena);
    void        AddWidget(String8 name, ImGuiRenderCallback callback);
    ImTextureID AddTexture(kraft::r::Handle<kraft::Texture> texture, kraft::r::Handle<kraft::TextureSampler> sampler);
    void        RemoveTexture(ImTextureID Texture);
    void        OnResize(int Width, int Height);
    void        RenderWidgets();
    void        Destroy();
    void        EndFrameUpdatePlatformWindows();

    void BeginFrame();
    void EndFrame();
};
