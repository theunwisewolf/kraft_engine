#pragma once

#include "core/kraft_core.h"
#include "containers/array.h"

namespace kraft
{

// "refresh" is true whenever the window has been resized
typedef void (*ImGuiRenderCallback)(bool refresh);

struct ImGuiWidget
{
    const char          Name[128] = "";
    ImGuiRenderCallback Callback  = nullptr;

    ImGuiWidget& operator=(const kraft::ImGuiWidget &widget)
    {
        MemCpy((void*)this->Name, (void*)widget.Name, sizeof(ImGuiWidget::Name));
        this->Callback = widget.Callback;

        return *this;
    }
};

struct RendererImGui
{
    ImGuiWidget* Widgets = 0;

    void Init();
    void AddWidget(const char* name, ImGuiRenderCallback callback);
    void OnResize(int width, int height);
    void RenderWidgets();
    void Destroy();
};

}