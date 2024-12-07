#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Window;
struct EngineConfig;
enum Keys : uint32;

struct KRAFT_API Platform
{
    // If the platform wants to store any internal state,
    // it should do so in this variable
    static void* InternalState;

    static bool Init(EngineConfig* config);
    static bool PollEvents();
    static void Shutdown();

    // Memory
    static void* Malloc(uint64_t size, bool aligned);
    static void* Realloc(void* region, uint64_t size, bool aligned);
    static void  Free(void* region, bool aligned);
    static void* MemZero(void* region, uint64_t size);
    static void* MemCpy(void* dst, const void* src, uint64_t size);
    static void* MemSet(void* region, int value, uint64_t size);
    static int   MemCmp(const void* a, const void* b, uint64_t size);

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

    static void ConsoleOutputString(const char* str, int color);
    static void ConsoleOutputStringError(const char* str, int color);

    // Time
    // Returns the absolute time in seconds
    static float64 GetAbsoluteTime();

    // Returns the elapsed time in milliseconds
    static float64 GetElapsedTime();

    // Misc
    static void        SleepMilliseconds(uint64_t Milliseconds);
    static const char* GetKeyName(Keys key);
    static Window&     GetWindow();

    static const char* GetEnv(const char* Key);
    static bool        ExecuteProcess(const char* WorkingDir, const char* ExecutablePath, const char** Args, char** Output);
};

}