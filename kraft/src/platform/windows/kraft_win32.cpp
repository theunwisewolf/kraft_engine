#include "kraft_win32.h"

#ifdef KRAFT_PLATFORM_WINDOWS

#include <Windows.h>
#include <memory>

#if defined(KRAFT_GUI_APP)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>
#endif

#include "core/kraft_log.h"
#include "core/kraft_string.h"
#include "core/kraft_application.h"
#include "platform/kraft_platform.h"

namespace kraft
{

Win32PlatformState* State                 = nullptr;
void* Platform::InternalState             = nullptr;

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
static HANDLE s_ConsoleOutputHandle;
static HANDLE s_ConsoleErrorHandle;
static CONSOLE_SCREEN_BUFFER_INFO s_ConsoleOutputScreenBufferInfo = {};
static CONSOLE_SCREEN_BUFFER_INFO s_ConsoleErrorScreenBufferInfo = {};

bool Platform::Init(ApplicationConfig* config)
{
    InternalState = Malloc(sizeof(Win32PlatformState), false);
    State = (Win32PlatformState*)InternalState;

#if defined(KRAFT_GUI_APP)
    State->Window.Init(config->WindowTitle, config->WindowWidth, config->WindowHeight, config->RendererBackend);
    State->hWindow = glfwGetWin32Window(State->Window.PlatformWindowHandle);
    State->hInstance = GetModuleHandleW(NULL);
#else
    State = (Win32PlatformState*)MemZero(State, sizeof(Win32PlatformState));
#endif

    QueryPerformanceFrequency(&s_ClockFrequency);
    QueryPerformanceCounter(&s_StartTime);

    s_ConsoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    s_ConsoleErrorHandle = GetStdHandle(STD_ERROR_HANDLE);

    GetConsoleScreenBufferInfo(s_ConsoleOutputHandle, &s_ConsoleOutputScreenBufferInfo);
    GetConsoleScreenBufferInfo(s_ConsoleErrorHandle, &s_ConsoleErrorScreenBufferInfo);

    return true;
}

bool Platform::PollEvents()
{
#if defined(KRAFT_GUI_APP)
    return State->Window.PollEvents();
#else
    return false;
#endif
}

void Platform::Shutdown()
{
#if defined(KRAFT_GUI_APP)
    State->Window.Destroy();
#endif
    Free(InternalState, false);
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

void *Platform::MemCpy(void *dst, const void *src, uint64_t size)
{
    return memcpy(dst, src, size);
}

void *Platform::MemSet(void *region, int value, uint64_t size)
{
    return memset(region, value, size);
}

int Platform::MemCmp(const void *a, const void *b, uint64_t size)
{
    return memcmp(a, b, size);
}

// ------------------------------------------ 
// Console Specific Functions
// ------------------------------------------ 

// https://docs.microsoft.com/en-us/windows/console/using-the-high-level-input-and-output-functions
void Platform::ConsoleOutputString(const char* str, int color)
{
    SetConsoleTextAttribute(s_ConsoleOutputHandle, color);
#ifdef UNICODE
    int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
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
    OutputDebugString(str);
    WriteConsole(s_ConsoleOutputHandle, str, (DWORD)StringLength(str), 0, NULL);
#endif

    // Reset console
    SetConsoleTextAttribute(s_ConsoleOutputHandle, s_ConsoleOutputScreenBufferInfo.wAttributes);
}

void Platform::ConsoleOutputStringError(const char* str, int color)
{
    SetConsoleTextAttribute(s_ConsoleErrorHandle, color);
#ifdef UNICODE
    int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
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
    OutputDebugString(str);
    WriteConsole(s_ConsoleErrorHandle, str, (DWORD)StringLength(str), 0, NULL);
#endif

    // Reset console
    SetConsoleTextAttribute(s_ConsoleErrorHandle, s_ConsoleErrorScreenBufferInfo.wAttributes);
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
    Sleep((DWORD)msec);
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
    HANDLE StdinPipeRead, StdinPipeWrite, StdoutPipeRead;

    STARTUPINFO StartupInfo = {};
	
	return false;
}

}

#include "platform/kraft_filesystem.h"

namespace kraft
{
namespace filesystem
{
bool ReadDir(const String& Path, Array<FileInfo>& OutFiles)
{
    String Dir = Path;
    if (!Dir.EndsWith("/*") && !Dir.EndsWith("\\*"))
    {
        Dir += "/*";
    }

    WIN32_FIND_DATA FindData;
    HANDLE hFind = FindFirstFile(*Dir, &FindData);
    LARGE_INTEGER FileSize;

    if (INVALID_HANDLE_VALUE == hFind) 
    {
        KERROR("[Win32]: FindFirstFile failed with error code %d", GetLastError());
        return false;
    }

    do
    {
        if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            KDEBUG("[Dir]: %s", FindData.cFileName);
        }
        else
        {
            FileSize.LowPart = FindData.nFileSizeLow;
            FileSize.HighPart = FindData.nFileSizeHigh;
            KDEBUG("  %s   %ld bytes", FindData.cFileName, FileSize.QuadPart);

            OutFiles.Push({ .Name = FindData.cFileName, .FileSize = (uint64)FileSize.QuadPart });
        }
    } while (FindNextFile(hFind, &FindData) != 0);

    FindClose(hFind);
    return true;
}
}
}

#endif