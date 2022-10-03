#pragma once

#include "core/kraft_core.h"

#ifdef KRAFT_PLATFORM_MACOS
#include "platform/kraft_window.h"

namespace kraft
{

struct MacOSPlatformState
{
    Window      Window;
};

}
#endif