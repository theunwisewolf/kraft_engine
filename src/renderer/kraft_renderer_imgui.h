#pragma once

#include "core/kraft_core.h"
#include "containers/array.h"

namespace kraft
{

typedef void (*ImGuiRenderCallback)();

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
    void RenderWidgets();
    void Destroy();
};

}