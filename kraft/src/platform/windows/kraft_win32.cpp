#include "kraft_win32.h"

#include <core/kraft_core.h>

#ifdef KRAFT_PLATFORM_WINDOWS

#pragma comment(lib, "kernel32")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commdlg.h>
#include <dbghelp.h>
#include <memory>
#include <stdio.h>
#pragma comment(lib, "comdlg32")
#pragma comment(lib, "dbghelp")

#if defined(KRAFT_GUI_APP)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>
#include <platform/kraft_window.h>
#endif

#include <core/kraft_allocators.h>
#include <core/kraft_string.h>
#include <core/kraft_strings.h>
#include <core/kraft_thread_context.h>
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

u64 Platform::TimeNowNS()
{
    FILETIME       ft;
    ULARGE_INTEGER uli;

    GetSystemTimePreciseAsFileTime(&ft);

    // Convert FILETIME (100-ns intervals since Jan 1 1601) into a 64-bit value
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    // Subtract the difference between 1601 and 1970 in 100-ns ticks:
    //   369 years + 89 leap days = 11644473600 seconds -> 11644473600 * 10^7 = 116444736000000000 (100-ns units)
    const u64 EPOCH_DIFF = 116444736000000000ULL;
    uli.QuadPart -= EPOCH_DIFF;

    // Now uli.QuadPart is the number of 100-ns ticks since 1970
    // Multiply by 100 to get nanoseconds
    return (u64)(uli.QuadPart * 100);
}

f64 Platform::GetClockTimeNS()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (f64)(now.QuadPart * 1000000000.0 / s_ClockFrequency.QuadPart);
}

f64 Platform::GetAbsoluteTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (f64)((f64)now.QuadPart / (f64)s_ClockFrequency.QuadPart);
}

f64 Platform::GetElapsedTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    LARGE_INTEGER elapsed;
    elapsed.QuadPart = now.QuadPart - s_StartTime.QuadPart;
    elapsed.QuadPart *= 1000; // Convert to ms to preserve precision

    return (f64)((f64)elapsed.QuadPart / (f64)s_ClockFrequency.QuadPart);
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

String8 Platform::ExecutableDirectory(ArenaAllocator* arena)
{
    char module_path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, module_path, (DWORD)sizeof(module_path));
    while (length > 0 && module_path[length - 1] != '\\' && module_path[length - 1] != '/')
        length--;
    if (length > 0)
        length--;

    return StringCopy(arena, String8{ { (u8*)module_path }, (u64)length });
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* info)
{
    fprintf(stderr, "\n[crash] exception 0x%08lX at %p\n", info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);

    ULONG_PTR stack_low = 0;
    ULONG_PTR stack_high = 0;
    GetCurrentThreadStackLimits(&stack_low, &stack_high);
    u64 module_base = (u64)GetModuleHandleA(nullptr);
    fprintf(stderr, "[crash] rip rva=0x%llX rsp=0x%llX stack=[0x%llX, 0x%llX) used=%llu KB%s\n",
        (unsigned long long)((u64)info->ExceptionRecord->ExceptionAddress - module_base),
        (unsigned long long)info->ContextRecord->Rsp,
        (unsigned long long)stack_low,
        (unsigned long long)stack_high,
        (unsigned long long)((stack_high - info->ContextRecord->Rsp) / 1024),
        info->ContextRecord->Rsp < stack_low ? "  <-- RSP BELOW STACK LIMIT" : "");
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
    {
        fprintf(stderr, "[crash] %s address 0x%llX\n",
            info->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
            (unsigned long long)info->ExceptionRecord->ExceptionInformation[1]);
    }

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT context = *info->ContextRecord;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (u32 depth = 0; depth < 48; depth++)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (frame.AddrPC.Offset == 0)
            break;

        char symbol_buffer[sizeof(SYMBOL_INFO) + 256] = {};
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbol_buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 symbol_displacement = 0;
        const char* name = SymFromAddr(process, frame.AddrPC.Offset, &symbol_displacement, symbol) ? symbol->Name : "?";

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD line_displacement = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &line))
            fprintf(stderr, "  %2u: %s  (%s:%lu)\n", depth, name, line.FileName, line.LineNumber);
        else
            fprintf(stderr, "  %2u: %s\n", depth, name);
    }
    fflush(stderr);

    return EXCEPTION_EXECUTE_HANDLER;
}

void Platform::InstallCrashHandler()
{
    SetUnhandledExceptionFilter(CrashHandler);
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

static wchar_t* DialogPushWide(ArenaAllocator* arena, const char* text)
{
    int wide_length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    wchar_t* wide = ArenaPushArrayNoZero(arena, wchar_t, (u64)wide_length + 1);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_length);
    wide[wide_length] = 0;

    return wide;
}

static wchar_t* DialogPushFilter(ArenaAllocator* arena, const char* filter)
{
    if (!filter || !filter[0])
        return nullptr;

    wchar_t* wide = DialogPushWide(arena, filter);
    for (wchar_t* character = wide; *character; character++)
    {
        if (*character == L'|')
            *character = 0;
    }

    return wide;
}

static String8 DialogPushUtf8(ArenaAllocator* arena, const wchar_t* wide)
{
    int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (utf8_length <= 0)
        utf8_length = 1;

    String8 result;
    result.ptr = ArenaPushArrayNoZero(arena, u8, (u64)utf8_length);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.str, utf8_length, nullptr, nullptr);
    result.count = (u64)utf8_length - 1;
    result.str[result.count] = 0;

    return result;
}

static HWND DialogOwnerWindow()
{
    if (Platform::State && Platform::State->PrimaryWindow && Platform::State->PrimaryWindow->PlatformWindowState)
        return Platform::State->PrimaryWindow->PlatformWindowState->hWindow;

    return nullptr;
}

bool Platform::OpenFileDialog(ArenaAllocator* arena, const char* filter, String8* out_path)
{
    TempArena scratch = ScratchBegin(&arena, 1);
    wchar_t path_buffer[2048];
    path_buffer[0] = 0;

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = DialogOwnerWindow();
    dialog.lpstrFilter = DialogPushFilter(scratch.arena, filter);
    dialog.lpstrFile = path_buffer;
    dialog.nMaxFile = (DWORD)KRAFT_C_ARRAY_SIZE(path_buffer);
    dialog.Flags = OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

    bool ok = GetOpenFileNameW(&dialog) != 0;
    if (ok)
        *out_path = DialogPushUtf8(arena, path_buffer);
    ScratchEnd(scratch);

    return ok;
}

bool Platform::SaveFileDialog(ArenaAllocator* arena, const char* filter, const char* default_extension, String8* out_path)
{
    TempArena scratch = ScratchBegin(&arena, 1);
    wchar_t path_buffer[2048];
    path_buffer[0] = 0;

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = DialogOwnerWindow();
    dialog.lpstrFilter = DialogPushFilter(scratch.arena, filter);
    dialog.lpstrFile = path_buffer;
    dialog.nMaxFile = (DWORD)KRAFT_C_ARRAY_SIZE(path_buffer);
    dialog.lpstrDefExt = default_extension ? DialogPushWide(scratch.arena, default_extension) : nullptr;
    dialog.Flags = OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

    bool ok = GetSaveFileNameW(&dialog) != 0;
    if (ok)
        *out_path = DialogPushUtf8(arena, path_buffer);
    ScratchEnd(scratch);

    return ok;
}


#endif

#endif
}