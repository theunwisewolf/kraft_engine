#pragma once

namespace kraft {

struct PlatformState
{
#if defined(KRAFT_GUI_APP)
    Window* PrimaryWindow = nullptr;
#endif
};

#if defined(KRAFT_GUI_APP)
struct PlatformWindowState
{
    HWND      hWindow = 0;
    HINSTANCE hInstance = 0;
};
#endif

} // namespace kraft