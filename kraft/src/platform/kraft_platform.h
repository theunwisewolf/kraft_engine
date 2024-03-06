#pragma once

#include <cstdint>

#include "core/kraft_core.h"
#include "core/kraft_input.h"
#include "platform/kraft_window.h"

#if defined(KRAFT_PLATFORM_WINDOWS)
#include "windows/kraft_win32.h"
#elif defined(KRAFT_PLATFORM_MACOS)
#include "macos/kraft_macos.h"
#endif

namespace kraft
{

struct ApplicationConfig;

struct KRAFT_API Platform
{
    // If the platform wants to store any internal state,
    // it should do so in this variable
    static void *InternalState;

    static bool Init(ApplicationConfig* config);
    static bool PollEvents();
    static void Shutdown();

    // Memory
    static void *Malloc(uint64_t size, bool aligned);
    static void *Realloc(void *region, uint64_t size, bool aligned);
    static void Free(void *region, bool aligned);
    static void *MemZero(void *region, uint64_t size);
    static void *MemCpy(void *dst, const void *src, uint64_t size);
    static void *MemSet(void *region, int value, uint64_t size);
    static int MemCmp(const void *a, const void *b, uint64_t size);

    // Console
    // Foreground colors
    static const int ConsoleColorBlack;
    static const int ConsoleColorLoWhite;
    static const int ConsoleColorHiWhite;
    static const int ConsoleColorLoRed;
    static const int ConsoleColorHiRed;
    static const int ConsoleColorLoGreen;
    static const int ConsoleColorHiGreen;
    static const int ConsoleColorLoBlue;
    static const int ConsoleColorHiBlue;
    static const int ConsoleColorLoYellow;
    static const int ConsoleColorHiYellow;
    static const int ConsoleColorLoCyan;
    static const int ConsoleColorHiCyan;
    static const int ConsoleColorLoMagenta;
    static const int ConsoleColorHiMagenta;

    // Background colors
    static const int ConsoleColorBGBlack;
    static const int ConsoleColorBGLoWhite;
    static const int ConsoleColorBGHiWhite;
    static const int ConsoleColorBGLoRed;
    static const int ConsoleColorBGHiRed;
    static const int ConsoleColorBGLoGreen;
    static const int ConsoleColorBGHiGreen;
    static const int ConsoleColorBGLoBlue;
    static const int ConsoleColorBGHiBlue;
    static const int ConsoleColorBGLoYellow;
    static const int ConsoleColorBGHiYellow;
    static const int ConsoleColorBGLoCyan;
    static const int ConsoleColorBGHiCyan;
    static const int ConsoleColorBGLoMagenta;
    static const int ConsoleColorBGHiMagenta;

    static void ConsoleOutputString(const char *str, int color);
    static void ConsoleOutputStringError(const char *str, int color);

    // Time
    static float64 GetAbsoluteTime();
    static float64 GetElapsedTime();

    // Misc
    static void SleepMilliseconds(uint64_t msec);
    static const char* GetKeyName(Keys key);
    static Window& GetWindow()
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

    static const char* GetEnv(const char* Key);
    static bool ExecuteProcess(const char* WorkingDir, const char* ExecutablePath, const char** Args, char** Output);
};

}