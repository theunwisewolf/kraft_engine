#include "kraft_filesystem.cpp"
#include "kraft_platform_common.cpp"
#include "kraft_window.cpp"

#if defined(KRAFT_PLATFORM_WINDOWS)
#include "windows/kraft_win32.cpp"
#include "windows/kraft_win32_filesystem.cpp"
#elif defined(KRAFT_PLATFORM_LINUX)
#include "linux/kraft_linux.cpp"
#elif defined(KRAFT_PLATFORM_MACOS)
#include "macos/kraft_macos.mm"
#endif