#include "kraft_renderer_imgui.h"

#include "core/kraft_log.h"

namespace kraft
{

static bool WidgetsNeedRefresh = false;

void RendererImGui::Init()
{
    // CreateArray(this->Widgets, 128);
}

void RendererImGui::AddWidget(const char* name, ImGuiRenderCallback callback)
{
    ImGuiWidget widget;
    MemCpy((void*)widget.Name, (void*)name, sizeof(ImGuiWidget::Name));
    widget.Callback = callback;

    arrpush(this->Widgets, widget);

    KINFO("Added imgui widget %s", name);
}

void RendererImGui::RenderWidgets()
{
    size_t size = arrlen(this->Widgets);
    for (int i = 0; i < size; i++)
    {
        this->Widgets[i].Callback(WidgetsNeedRefresh);
    }

    WidgetsNeedRefresh = false;
}

void RendererImGui::OnResize(int width, int height)
{
    WidgetsNeedRefresh = true;
}

void RendererImGui::Destroy()
{
    arrfree(this->Widgets);
}

}