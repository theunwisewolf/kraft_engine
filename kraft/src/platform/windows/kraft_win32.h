#pragma once

#include "core/kraft_core.h"

#ifdef KRAFT_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include "platform/kraft_window.h"

namespace kraft
{

struct Win32PlatformState
{
    Window      Window;
    HWND        hWindow;
    HINSTANCE   hInstance;
};

}
#endif