#include "platform/kraft_platform.h"

#ifdef KRAFT_PLATFORM_WINDOWS

#include <memory>
#include <Windows.h>

#include "core/kraft_log.h"

namespace kraft
{

void* Platform::InternalState       = nullptr;
const int Platform::ConsoleColorBlack     = 0;
const int Platform::ConsoleColorLoWhite   = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
const int Platform::ConsoleColorHiWhite   = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoRed     = FOREGROUND_RED;
const int Platform::ConsoleColorHiRed     = FOREGROUND_RED | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoGreen   = FOREGROUND_GREEN;
const int Platform::ConsoleColorHiGreen   = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoBlue    = FOREGROUND_BLUE;
const int Platform::ConsoleColorHiBlue    = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoYellow  = Platform::ConsoleColorLoRed  | Platform::ConsoleColorLoGreen;
const int Platform::ConsoleColorHiYellow  = Platform::ConsoleColorHiRed  | Platform::ConsoleColorHiGreen;
const int Platform::ConsoleColorLoCyan    = Platform::ConsoleColorLoBlue | Platform::ConsoleColorLoGreen;
const int Platform::ConsoleColorHiCyan    = Platform::ConsoleColorHiBlue | Platform::ConsoleColorHiGreen;
const int Platform::ConsoleColorLoMagenta = Platform::ConsoleColorLoBlue | Platform::ConsoleColorLoRed;
const int Platform::ConsoleColorHiMagenta = Platform::ConsoleColorHiBlue | Platform::ConsoleColorHiRed;

const int Platform::ConsoleColorBGBlack     = 0;
const int Platform::ConsoleColorBGLoWhite   = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
const int Platform::ConsoleColorBGHiWhite   = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoRed     = BACKGROUND_RED;
const int Platform::ConsoleColorBGHiRed     = BACKGROUND_RED | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoGreen   = BACKGROUND_GREEN;
const int Platform::ConsoleColorBGHiGreen   = BACKGROUND_GREEN | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoBlue    = BACKGROUND_BLUE;
const int Platform::ConsoleColorBGHiBlue    = BACKGROUND_BLUE | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoYellow  = Platform::ConsoleColorBGLoRed  | Platform::ConsoleColorBGLoGreen;
const int Platform::ConsoleColorBGHiYellow  = Platform::ConsoleColorBGHiRed  | Platform::ConsoleColorBGHiGreen;
const int Platform::ConsoleColorBGLoCyan    = Platform::ConsoleColorBGLoBlue | Platform::ConsoleColorBGLoGreen;
const int Platform::ConsoleColorBGHiCyan    = Platform::ConsoleColorBGHiBlue | Platform::ConsoleColorBGHiGreen;
const int Platform::ConsoleColorBGLoMagenta = Platform::ConsoleColorBGLoBlue | Platform::ConsoleColorBGLoRed;
const int Platform::ConsoleColorBGHiMagenta = Platform::ConsoleColorBGHiBlue | Platform::ConsoleColorBGHiRed;

static LARGE_INTEGER s_StartTime;
static LARGE_INTEGER s_ClockFrequency;

bool Platform::Init()
{
    QueryPerformanceFrequency(&s_ClockFrequency);
    QueryPerformanceCounter(&s_StartTime);

    return true;
}

void Platform::Shutdown()
{

}

// ------------------------------------------ 
// Memory Specific Functions
// ------------------------------------------

void *Platform::Malloc(uint64_t size, bool aligned)
{
    return malloc(size);
}

void *Platform::Realloc(void *region, uint64_t size, bool aligned)
{
    return realloc(region, size);
}

void Platform::Free(void *region, bool aligned)
{
    free(region);
}

void *Platform::MemZero(void *region, uint64_t size)
{
    return memset(region, 0, size);
}

void *Platform::MemCpy(void *dst, void *src, uint64_t size)
{
    return memcpy(dst, src, size);
}

void *Platform::MemSet(void *region, int value, uint64_t size)
{
    return memset(region, value, size);
}

// ------------------------------------------ 
// Console Specific Functions
// ------------------------------------------ 

// https://docs.microsoft.com/en-us/windows/console/using-the-high-level-input-and-output-functions
void Platform::ConsoleOutputString(const char* str, int color)
{
    // Get a handle to STDOUT
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    
    SetConsoleTextAttribute(out, color);
    OutputDebugString(str);
    WriteConsole(out, str, strlen(str), 0, NULL);
}

void Platform::ConsoleOutputStringError(const char* str, int color)
{
    // Get a handle to STDERR
    HANDLE out = GetStdHandle(STD_ERROR_HANDLE);
    
    SetConsoleTextAttribute(out, color);
    OutputDebugString(str);
    WriteConsole(out, str, strlen(str), 0, NULL);
}

float64 Platform::GetAbsoluteTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (float64)((float64)now.QuadPart / (float64)s_ClockFrequency.QuadPart);
}

float64 Platform::GetElapsedTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    LARGE_INTEGER elapsed;
    elapsed.QuadPart = now.QuadPart - s_StartTime.QuadPart;
    elapsed.QuadPart *= 1000; // Convert to ms to preserve precision

    return (float64)((float64)elapsed.QuadPart / (float64)s_ClockFrequency.QuadPart);
}

void Platform::SleepMilliseconds(uint64_t msec)
{
    Sleep(msec);
}

}

#endif