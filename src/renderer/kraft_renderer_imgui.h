#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "containers/kraft_array.h"

namespace kraft
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

    ImGuiWidget& operator=(const kraft::ImGuiWidget &Widget)
    {
        Name = Widget.Name;
        Callback = Widget.Callback;

        return *this;
    }
};

struct RendererImGui
{
    Array<ImGuiWidget> Widgets;

    void Init();
    void AddWidget(const String& Name, ImGuiRenderCallback Callback);
    void OnResize(int width, int height);
    void RenderWidgets();
    void Destroy();
};

}