#include "kraft_renderer_imgui.h"

#include "core/kraft_log.h"

namespace kraft
{

static bool WidgetsNeedRefresh = false;

void RendererImGui::Init()
{
}

void RendererImGui::AddWidget(const TString& Name, ImGuiRenderCallback Callback)
{
    ImGuiWidget Widget(Name, Callback);
    Widgets.Push(Widget);

    KINFO(TEXT("Added imgui widget %s"), *Name);
}

void RendererImGui::RenderWidgets()
{
    uint64 Size = Widgets.Length;
    for (int i = 0; i < Size; i++)
    {
        Widgets[i].Callback(WidgetsNeedRefresh);
    }

    WidgetsNeedRefresh = false;
}

void RendererImGui::OnResize(int width, int height)
{
    WidgetsNeedRefresh = true;
}

void RendererImGui::Destroy()
{
}

}