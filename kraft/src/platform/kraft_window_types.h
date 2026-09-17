#pragma once

#include <core/kraft_core.h>
#include <core/kraft_string.h>

namespace kraft {

enum CursorMode {
    CURSOR_MODE_NORMAL = 0x00034001,
    CURSOR_MODE_HIDDEN = 0x00034002,
    CURSOR_MODE_DISABLED = 0x00034003,
    CURSOR_MODE_LOCKED_TO_WINDOW = 0x00034004,
};

// Same values as GLFW_*_CURSOR
enum CursorShape {
    CURSOR_SHAPE_ARROW = 0x00036001,
    CURSOR_SHAPE_IBEAM = 0x00036002,
    CURSOR_SHAPE_CROSSHAIR = 0x00036003,
    CURSOR_SHAPE_HAND = 0x00036004,
    CURSOR_SHAPE_RESIZE_HORIZONTAL = 0x00036005,
    CURSOR_SHAPE_RESIZE_VERTICAL = 0x00036006,
    CURSOR_SHAPE_RESIZE_DIAGONAL_NWSE = 0x00036007,
    CURSOR_SHAPE_RESIZE_DIAGONAL_NESW = 0x00036008,
    CURSOR_SHAPE_RESIZE_ALL = 0x00036009,
    CURSOR_SHAPE_NOT_ALLOWED = 0x0003600A,
    CURSOR_SHAPE_COUNT = 10,
};

struct WindowOptions {
    String Title = "";
    u32 Width = 0;
    u32 Height = 0;
    int RenderererHint = 1;
    bool Primary = true;
    bool StartMaximized = false;
    bool CustomTitleBar = false;
};

} // namespace kraft
