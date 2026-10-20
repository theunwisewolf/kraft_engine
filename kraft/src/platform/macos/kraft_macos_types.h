#pragma once

namespace kraft {

struct PlatformState {
#if defined(KRAFT_GUI_APP)
    Window* PrimaryWindow = nullptr;
#endif
};

#if defined(KRAFT_GUI_APP)
struct PlatformWindowState {
    void* ns_window; // NSWindow*
};
#endif

} // namespace kraft
