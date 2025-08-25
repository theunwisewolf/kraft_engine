#include "kraft_win32.h"

#include <core/kraft_core.h>

#ifdef KRAFT_PLATFORM_WINDOWS

#pragma comment(lib, "kernel32")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <memory>

#if defined(KRAFT_GUI_APP)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>
#include <platform/kraft_window.h>
#endif

#include <core/kraft_string.h>
#include <platform/kraft_platform.h>
#include <platform/windows/kraft_win32_types.h>

namespace kraft {

PlatformState* Platform::State = nullptr;

const int Platform::ConsoleColorBlack = 0;
const int Platform::ConsoleColorLoWhite = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
const int Platform::ConsoleColorHiWhite = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoRed = FOREGROUND_RED;
const int Platform::ConsoleColorHiRed = FOREGROUND_RED | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoGreen = FOREGROUND_GREEN;
const int Platform::ConsoleColorHiGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoBlue = FOREGROUND_BLUE;
const int Platform::ConsoleColorHiBlue = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const int Platform::ConsoleColorLoYellow = Platform::ConsoleColorLoRed | Platform::ConsoleColorLoGreen;
const int Platform::ConsoleColorHiYellow = Platform::ConsoleColorHiRed | Platform::ConsoleColorHiGreen;
const int Platform::ConsoleColorLoCyan = Platform::ConsoleColorLoBlue | Platform::ConsoleColorLoGreen;
const int Platform::ConsoleColorHiCyan = Platform::ConsoleColorHiBlue | Platform::ConsoleColorHiGreen;
const int Platform::ConsoleColorLoMagenta = Platform::ConsoleColorLoBlue | Platform::ConsoleColorLoRed;
const int Platform::ConsoleColorHiMagenta = Platform::ConsoleColorHiBlue | Platform::ConsoleColorHiRed;
const int Platform::ConsoleColorLoGray = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
const int Platform::ConsoleColorHiGray = FOREGROUND_INTENSITY;

const int Platform::ConsoleColorBGBlack = 0;
const int Platform::ConsoleColorBGLoWhite = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
const int Platform::ConsoleColorBGHiWhite = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoRed = BACKGROUND_RED;
const int Platform::ConsoleColorBGHiRed = BACKGROUND_RED | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoGreen = BACKGROUND_GREEN;
const int Platform::ConsoleColorBGHiGreen = BACKGROUND_GREEN | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoBlue = BACKGROUND_BLUE;
const int Platform::ConsoleColorBGHiBlue = BACKGROUND_BLUE | BACKGROUND_INTENSITY;
const int Platform::ConsoleColorBGLoYellow = Platform::ConsoleColorBGLoRed | Platform::ConsoleColorBGLoGreen;
const int Platform::ConsoleColorBGHiYellow = Platform::ConsoleColorBGHiRed | Platform::ConsoleColorBGHiGreen;
const int Platform::ConsoleColorBGLoCyan = Platform::ConsoleColorBGLoBlue | Platform::ConsoleColorBGLoGreen;
const int Platform::ConsoleColorBGHiCyan = Platform::ConsoleColorBGHiBlue | Platform::ConsoleColorBGHiGreen;
const int Platform::ConsoleColorBGLoMagenta = Platform::ConsoleColorBGLoBlue | Platform::ConsoleColorBGLoRed;
const int Platform::ConsoleColorBGHiMagenta = Platform::ConsoleColorBGHiBlue | Platform::ConsoleColorBGHiRed;

static LARGE_INTEGER              s_StartTime;
static LARGE_INTEGER              s_ClockFrequency;
static HANDLE                     s_ConsoleOutputHandle;
static HANDLE                     s_ConsoleErrorHandle;
static CONSOLE_SCREEN_BUFFER_INFO s_ConsoleOutputScreenBufferInfo = {};
static CONSOLE_SCREEN_BUFFER_INFO s_ConsoleErrorScreenBufferInfo = {};

static void LogLastError()
{
    kraft::String ErrorMessage(1024, 0);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), *ErrorMessage, ErrorMessage.Length, NULL);

    Platform::ConsoleOutputString(*ErrorMessage);
}

bool Platform::Init(struct EngineConfig* config)
{
    State = (PlatformState*)Malloc(sizeof(PlatformState), false);
    MemZero(State, sizeof(PlatformState));

    QueryPerformanceFrequency(&s_ClockFrequency);
    QueryPerformanceCounter(&s_StartTime);

    s_ConsoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    s_ConsoleErrorHandle = GetStdHandle(STD_ERROR_HANDLE);

    DWORD ConsoleMode = 0;
    if (!GetConsoleMode(s_ConsoleOutputHandle, &ConsoleMode))
    {
        LogLastError();
        return false;
    }

    ConsoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(s_ConsoleOutputHandle, ConsoleMode))
    {
        LogLastError();
        return false;
    }

    ConsoleMode = 0;
    if (!GetConsoleMode(s_ConsoleErrorHandle, &ConsoleMode))
    {
        LogLastError();
        return false;
    }

    ConsoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(s_ConsoleErrorHandle, ConsoleMode))
    {
        LogLastError();
        return false;
    }

    GetConsoleScreenBufferInfo(s_ConsoleOutputHandle, &s_ConsoleOutputScreenBufferInfo);
    GetConsoleScreenBufferInfo(s_ConsoleErrorHandle, &s_ConsoleErrorScreenBufferInfo);

    return true;
}

bool Platform::PollEvents()
{
#if defined(KRAFT_GUI_APP)
    return State->PrimaryWindow->PollEvents();
#else
    return false;
#endif
}

void Platform::Shutdown()
{
#if defined(KRAFT_GUI_APP)
    DestroyPlatformWindow(State->PrimaryWindow);
#endif

    Free(State);
}

// ------------------------------------------
// Memory Specific Functions
// ------------------------------------------

void* Platform::Malloc(uint64_t size, bool aligned)
{
    return malloc(size);
}

void* Platform::Realloc(void* region, uint64_t size, bool aligned)
{
    return realloc(region, size);
}

void Platform::Free(void* region)
{
    free(region);
}

void* Platform::MemZero(void* region, uint64_t size)
{
    return memset(region, 0, size);
}

void* Platform::MemCpy(void* dst, const void* src, uint64_t size)
{
    return memcpy(dst, src, size);
}

void* Platform::MemSet(void* region, int value, uint64_t size)
{
    return memset(region, value, size);
}

int Platform::MemCmp(const void* a, const void* b, uint64_t size)
{
    return memcmp(a, b, size);
}

// ------------------------------------------
// Console Specific Functions
// ------------------------------------------

void Platform::ConsoleSetColor(int Color)
{
    SetConsoleTextAttribute(s_ConsoleOutputHandle, Color);
}

// https://docs.microsoft.com/en-us/windows/console/using-the-high-level-input-and-output-functions
void Platform::ConsoleOutputString(const char* Str)
{
#ifdef UNICODE
    int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, Str, -1, NULL, 0);
    if (!CharacterCount)
    {
        OutputDebugString(L"MultiByteToWideChar failed to get character count");
        return;
    }

    WString WideString(CharacterCount, 0);
    if (!MultiByteToWideChar(CP_UTF8, 0, str, -1, *WideString, CharacterCount))
    {
        OutputDebugString(L"MultiByteToWideChar failed");
        return;
    }

    OutputDebugString(*WideString);
    WriteConsole(s_ConsoleOutputHandle, *WideString, (DWORD)WideString.Length, 0, NULL);
#else
    OutputDebugString(Str);
    WriteConsole(s_ConsoleOutputHandle, Str, (DWORD)StringLength(Str), 0, NULL);
#endif
}

void Platform::ConsoleOutputString(const char* Str, int Color)
{
    ConsoleSetColor(Color);
    ConsoleOutputString(Str);
    ConsoleResetFormatting();
}

void Platform::ConsoleOutputStringError(const char* Str)
{
#ifdef UNICODE
    int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, Str, -1, NULL, 0);
    if (!CharacterCount)
    {
        OutputDebugString(L"MultiByteToWideChar failed to get character count");
        return;
    }

    WString WideString(CharacterCount, 0);
    if (!MultiByteToWideChar(CP_UTF8, 0, str, -1, *WideString, CharacterCount))
    {
        OutputDebugString(L"MultiByteToWideChar failed");
        return;
    }

    OutputDebugString(*WideString);
    WriteConsole(s_ConsoleErrorHandle, *WideString, (DWORD)WideString.Length, 0, NULL);
#else
    OutputDebugString(Str);
    WriteConsole(s_ConsoleErrorHandle, Str, (DWORD)StringLength(Str), 0, NULL);
#endif
}

void Platform::ConsoleOutputStringError(const char* Str, int Color)
{
    ConsoleSetColor(Color);
    ConsoleOutputStringError(Str);
    ConsoleResetFormatting();
}

void Platform::ConsoleResetFormatting()
{
    SetConsoleTextAttribute(s_ConsoleErrorHandle, s_ConsoleErrorScreenBufferInfo.wAttributes);
}

uint64 Platform::TimeNowNS()
{
    FILETIME       ft;
    ULARGE_INTEGER uli;

    GetSystemTimePreciseAsFileTime(&ft);

    // Convert FILETIME (100-ns intervals since Jan 1 1601) into a 64-bit value
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    // Subtract the difference between 1601 and 1970 in 100-ns ticks:
    //   369 years + 89 leap days = 11644473600 seconds -> 11644473600 * 10^7 = 116444736000000000 (100-ns units)
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    uli.QuadPart -= EPOCH_DIFF;

    // Now uli.QuadPart is the number of 100-ns ticks since 1970
    // Multiply by 100 to get nanoseconds
    return (uint64)(uli.QuadPart * 100);
}

float64 Platform::GetClockTimeNS()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (float64)(now.QuadPart * 1000000000.0 / s_ClockFrequency.QuadPart);
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

void Platform::SleepMilliseconds(uint64_t Milliseconds)
{
    Sleep((DWORD)Milliseconds);
}

const char* Platform::GetKeyName(Keys key)
{
#if defined(KRAFT_GUI_APP)
    int keycode = (int)key;
    return glfwGetKeyName(keycode, glfwGetKeyScancode(keycode));
#else
    return "";
#endif
}

const char* Platform::GetEnv(const char* Key)
{
    return getenv(Key);
}

bool ExecuteProcess(const char* WorkingDir, const char* ExecutablePath, const char** Args, char** Output)
{
    return false;
}

#if defined(KRAFT_GUI_APP)
Window* Platform::CreatePlatformWindow(const WindowOptions* Opts)
{
    State->PrimaryWindow = (Window*)Malloc(sizeof(Window), false);
    State->PrimaryWindow->Init(Opts);

    State->PrimaryWindow->PlatformWindowState = (PlatformWindowState*)Malloc(sizeof(PlatformWindowState), false);
    MemSet(State->PrimaryWindow->PlatformWindowState, 0, sizeof(PlatformWindowState));
    State->PrimaryWindow->PlatformWindowState->hWindow = glfwGetWin32Window(State->PrimaryWindow->PlatformWindowHandle);
    State->PrimaryWindow->PlatformWindowState->hInstance = GetModuleHandleW(NULL);

    return State->PrimaryWindow;
}

void Platform::DestroyPlatformWindow(Window* Window)
{
    State->PrimaryWindow->Destroy();

    Free(State->PrimaryWindow->PlatformWindowState);
    Free(State->PrimaryWindow);
}

#endif

#endif
}