#include "platform/kraft_platform.h"

#ifdef KRAFT_PLATFORM_WINDOWS

#include <memory>
#include <Windows.h>

namespace kraft
{

void* Platform::InternalState       = nullptr;
int Platform::ConsoleColorRed       = FOREGROUND_RED | FOREGROUND_INTENSITY;
int Platform::ConsoleColorGreen     = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
int Platform::ConsoleColorBlue      = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
int Platform::ConsoleColorBGRed     = BACKGROUND_RED;
int Platform::ConsoleColorBGGreen   = BACKGROUND_GREEN;
int Platform::ConsoleColorBGBlue    = BACKGROUND_BLUE;

static LARGE_INTEGER s_StartTime;
static double s_ClockFrequency;

bool Platform::Init()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    s_ClockFrequency = 1.0 / (double)frequency.QuadPart;
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

float Platform::GetTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    
    return (float)((float)now.QuadPart * s_ClockFrequency);
}

void SleepMilliseconds(uint64_t msec)
{
    Sleep(msec);
}

}

#endif