#include "kraft_platform.h"

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <platform/kraft_window.h>
#include <platform/windows/kraft_win32_types.h>

#if defined(KRAFT_PLATFORM_WINDOWS)
#include "platform/windows/kraft_win32.h"
#elif defined(KRAFT_PLATFORM_MACOS)
#include "platform/macos/kraft_macos.h"
#endif

namespace kraft {

Window& Platform::GetWindow()
{
#if defined(KRAFT_PLATFORM_WINDOWS)
    Win32PlatformState* State = (Win32PlatformState*)Platform::InternalState;
#elif defined(KRAFT_PLATFORM_MACOS)
    MacOSPlatformState* State = (MacOSPlatformState*)Platform::InternalState;
#elif defined(KRAFT_PLATFORM_LINUX)
    LinuxPlatformState* State = (LinuxPlatformState*)Platform::InternalState;
#endif

    return State->Window;
}

}