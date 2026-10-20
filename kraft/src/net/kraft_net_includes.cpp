#include "kraft_connection.cpp"

#if defined(KRAFT_PLATFORM_WINDOWS)
#include "windows/kraft_win32_http.cpp"
#include "windows/kraft_win32_connection.cpp"
#elif defined(KRAFT_PLATFORM_MACOS)
#include "macos/kraft_macos_http.mm"
#include "macos/kraft_macos_connection.mm"
#endif
