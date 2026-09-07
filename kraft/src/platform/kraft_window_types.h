#pragma once

#include <core/kraft_core.h>
#include <core/kraft_string.h>

namespace kraft {

enum CursorMode
{
    CURSOR_MODE_NORMAL = 0x00034001,
    CURSOR_MODE_HIDDEN = 0x00034002,
    CURSOR_MODE_DISABLED = 0x00034003,
    CURSOR_MODE_LOCKED_TO_WINDOW = 0x00034004,
};

struct WindowOptions
{
    String Title = "";
    u32    Width = 0;
    u32    Height = 0;
    int    RenderererHint = 1;
    bool   Primary = true;
    bool   StartMaximized = false;
};

} // namespace kraft
