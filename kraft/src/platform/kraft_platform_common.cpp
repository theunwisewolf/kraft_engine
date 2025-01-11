#include "kraft_platform.h"

#if defined(KRAFT_GUI_APP)
#include <platform/kraft_window.h>
#endif

#if defined(KRAFT_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <platform/windows/kraft_win32_types.h>
#elif defined(KRAFT_PLATFORM_MACOS)
#include <platform/macos/kraft_macos_types.h>
#elif defined(KRAFT_PLATFORM_LINUX)
#include <platform/linux/kraft_linux_types.h>
#endif

namespace kraft {

#if defined(KRAFT_GUI_APP)
Window* Platform::GetWindow()
{
    return State->PrimaryWindow;
}
#endif

}