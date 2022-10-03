#pragma once

#include "core/kraft_core.h"
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