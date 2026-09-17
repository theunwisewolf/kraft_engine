#include "kraft_win32.h"

#include <core/kraft_core.h>

#ifdef KRAFT_PLATFORM_WINDOWS

#pragma comment(lib, "kernel32")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <dbghelp.h>
#include <memory>
#include <stdio.h>
#pragma comment(lib, "comdlg32")
#pragma comment(lib, "ole32")
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

static LARGE_INTEGER s_StartTime;
static LARGE_INTEGER s_ClockFrequency;
static HANDLE s_ConsoleOutputHandle;
static HANDLE s_ConsoleErrorHandle;
static CONSOLE_SCREEN_BUFFER_INFO s_ConsoleOutputScreenBufferInfo = {};
static CONSOLE_SCREEN_BUFFER_INFO s_ConsoleErrorScreenBufferInfo = {};

static void LogLastError() {
    kraft::String ErrorMessage(1024, 0);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), *ErrorMessage, ErrorMessage.Length, NULL);

    Platform::ConsoleOutputString(*ErrorMessage);
}

bool Platform::Init(struct EngineConfig* config) {
    State = (PlatformState*)Malloc(sizeof(PlatformState), false);
    MemZero(State, sizeof(PlatformState));

    QueryPerformanceFrequency(&s_ClockFrequency);
    QueryPerformanceCounter(&s_StartTime);

    s_ConsoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    s_ConsoleErrorHandle = GetStdHandle(STD_ERROR_HANDLE);

    DWORD ConsoleMode = 0;
    if (!GetConsoleMode(s_ConsoleOutputHandle, &ConsoleMode)) {
        LogLastError();
        return false;
    }

    ConsoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(s_ConsoleOutputHandle, ConsoleMode)) {
        LogLastError();
        return false;
    }

    ConsoleMode = 0;
    if (!GetConsoleMode(s_ConsoleErrorHandle, &ConsoleMode)) {
        LogLastError();
        return false;
    }

    ConsoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(s_ConsoleErrorHandle, ConsoleMode)) {
        LogLastError();
        return false;
    }

    GetConsoleScreenBufferInfo(s_ConsoleOutputHandle, &s_ConsoleOutputScreenBufferInfo);
    GetConsoleScreenBufferInfo(s_ConsoleErrorHandle, &s_ConsoleErrorScreenBufferInfo);

    return true;
}

bool Platform::PollEvents() {
#if defined(KRAFT_GUI_APP)
    return State->PrimaryWindow->PollEvents();
#else
    return false;
#endif
}

void Platform::Shutdown() {
#if defined(KRAFT_GUI_APP)
    DestroyPlatformWindow(State->PrimaryWindow);
#endif

    Free(State);
}

// ------------------------------------------
// Memory Specific Functions
// ------------------------------------------

void* Platform::Malloc(uint64_t size, bool aligned) {
    return malloc(size);
}

void* Platform::Realloc(void* region, uint64_t size, bool aligned) {
    return realloc(region, size);
}

void Platform::Free(void* region) {
    free(region);
}

void* Platform::MemZero(void* region, uint64_t size) {
    return memset(region, 0, size);
}

void* Platform::MemCpy(void* dst, const void* src, uint64_t size) {
    return memcpy(dst, src, size);
}

void* Platform::MemSet(void* region, int value, uint64_t size) {
    return memset(region, value, size);
}

int Platform::MemCmp(const void* a, const void* b, uint64_t size) {
    return memcmp(a, b, size);
}

// ------------------------------------------
// Console Specific Functions
// ------------------------------------------

void Platform::ConsoleSetColor(int Color) {
    SetConsoleTextAttribute(s_ConsoleOutputHandle, Color);
}

// https://docs.microsoft.com/en-us/windows/console/using-the-high-level-input-and-output-functions
void Platform::ConsoleOutputString(const char* Str) {
#ifdef UNICODE
    int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, Str, -1, NULL, 0);
    if (!CharacterCount) {
        OutputDebugString(L"MultiByteToWideChar failed to get character count");
        return;
    }

    WString WideString(CharacterCount, 0);
    if (!MultiByteToWideChar(CP_UTF8, 0, str, -1, *WideString, CharacterCount)) {
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

void Platform::ConsoleOutputString(const char* Str, int Color) {
    ConsoleSetColor(Color);
    ConsoleOutputString(Str);
    ConsoleResetFormatting();
}

void Platform::ConsoleOutputStringError(const char* Str) {
#ifdef UNICODE
    int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, Str, -1, NULL, 0);
    if (!CharacterCount) {
        OutputDebugString(L"MultiByteToWideChar failed to get character count");
        return;
    }

    WString WideString(CharacterCount, 0);
    if (!MultiByteToWideChar(CP_UTF8, 0, str, -1, *WideString, CharacterCount)) {
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

void Platform::ConsoleOutputStringError(const char* Str, int Color) {
    ConsoleSetColor(Color);
    ConsoleOutputStringError(Str);
    ConsoleResetFormatting();
}

void Platform::ConsoleResetFormatting() {
    SetConsoleTextAttribute(s_ConsoleErrorHandle, s_ConsoleErrorScreenBufferInfo.wAttributes);
}

u64 Platform::TimeNowNS() {
    FILETIME ft;
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

f64 Platform::GetClockTimeNS() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (f64)(now.QuadPart * 1000000000.0 / s_ClockFrequency.QuadPart);
}

f64 Platform::GetAbsoluteTime() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (f64)((f64)now.QuadPart / (f64)s_ClockFrequency.QuadPart);
}

f64 Platform::GetElapsedTime() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    LARGE_INTEGER elapsed;
    elapsed.QuadPart = now.QuadPart - s_StartTime.QuadPart;
    elapsed.QuadPart *= 1000; // Convert to ms to preserve precision

    return (f64)((f64)elapsed.QuadPart / (f64)s_ClockFrequency.QuadPart);
}

void Platform::SleepMilliseconds(uint64_t Milliseconds) {
    Sleep((DWORD)Milliseconds);
}

const char* Platform::GetKeyName(Keys key) {
#if defined(KRAFT_GUI_APP)
    int keycode = (int)key;
    return glfwGetKeyName(keycode, glfwGetKeyScancode(keycode));
#else
    return "";
#endif
}

const char* Platform::GetEnv(const char* Key) {
    return getenv(Key);
}

bool ExecuteProcess(const char* WorkingDir, const char* ExecutablePath, const char** Args, char** Output) {
    return false;
}

String8 Platform::ExecutableDirectory(ArenaAllocator* arena) {
    char module_path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, module_path, (DWORD)sizeof(module_path));
    while (length > 0 && module_path[length - 1] != '\\' && module_path[length - 1] != '/')
        length--;
    if (length > 0)
        length--;

    return StringCopy(arena, String8{{(u8*)module_path}, (u64)length});
}

String8 Platform::SystemFontDirectory(ArenaAllocator* arena) {
    char windows_directory[MAX_PATH];
    UINT length = GetWindowsDirectoryA(windows_directory, (UINT)sizeof(windows_directory));
    return StringFormat(arena, "%.*s/Fonts", (int)length, windows_directory);
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* info) {
    fprintf(stderr, "\n[crash] exception 0x%08lX at %p\n", info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);

    ULONG_PTR stack_low = 0;
    ULONG_PTR stack_high = 0;
    GetCurrentThreadStackLimits(&stack_low, &stack_high);
    u64 module_base = (u64)GetModuleHandleA(nullptr);
    fprintf(
        stderr,
        "[crash] rip rva=0x%llX rsp=0x%llX stack=[0x%llX, 0x%llX) used=%llu KB%s\n",
        (unsigned long long)((u64)info->ExceptionRecord->ExceptionAddress - module_base),
        (unsigned long long)info->ContextRecord->Rsp,
        (unsigned long long)stack_low,
        (unsigned long long)stack_high,
        (unsigned long long)((stack_high - info->ContextRecord->Rsp) / 1024),
        info->ContextRecord->Rsp < stack_low ? "  <-- RSP BELOW STACK LIMIT" : ""
    );
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        fprintf(stderr, "[crash] %s address 0x%llX\n", info->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from", (unsigned long long)info->ExceptionRecord->ExceptionInformation[1]);
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

    for (u32 depth = 0; depth < 48; depth++) {
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

void Platform::InstallCrashHandler() {
    SetUnhandledExceptionFilter(CrashHandler);
}

#if defined(KRAFT_GUI_APP)
static const wchar_t* KRAFT_WINDOW_PROPERTY = L"KraftWindow";

static int CustomFrameBorderThickness(HWND window_handle) {
    UINT dpi = GetDpiForWindow(window_handle);
    return GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

// Keeps the OS frame (resize borders, snap, shadow, double-click to maximize) but hands the caption
// area to the client so the app can draw its own title bar. See Window::SetCaptionRegion
static LRESULT CALLBACK CustomFrameWindowProc(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam) {
    Window* window = (Window*)GetPropW(window_handle, KRAFT_WINDOW_PROPERTY);
    WNDPROC original_proc = window ? window->PlatformWindowState->OriginalWindowProc : nullptr;
    if (!original_proc) {
        return DefWindowProcW(window_handle, message, wparam, lparam);
    }

    switch (message) {
    case WM_NCCALCSIZE: {
        if (!wparam)
            break;

        NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lparam;
        LONG original_top = params->rgrc[0].top;
        LRESULT result = CallWindowProcW(original_proc, window_handle, message, wparam, lparam);
        params->rgrc[0].top = original_top;
        if (IsZoomed(window_handle)) {
            params->rgrc[0].top += CustomFrameBorderThickness(window_handle);
        }

        return result;
    }

    case WM_NCHITTEST: {
        LRESULT hit = CallWindowProcW(original_proc, window_handle, message, wparam, lparam);
        if (hit != HTCLIENT)
            return hit;

        POINT point = {(short)LOWORD(lparam), (short)HIWORD(lparam)};
        ScreenToClient(window_handle, &point);

        int border = CustomFrameBorderThickness(window_handle);
        if (!IsZoomed(window_handle) && point.y < border) {
            if (point.x < border)
                return HTTOPLEFT;
            if (point.x >= window->FramebufferWidth - border)
                return HTTOPRIGHT;

            return HTTOP;
        }

        if ((f32)point.y < window->CaptionHeight) {
            Vec2f client_point((f32)point.x, (f32)point.y);
            for (u32 exclusion_index = 0; exclusion_index < window->CaptionExclusionCount; exclusion_index++) {
                if (RectContains(window->CaptionExclusions[exclusion_index], client_point)) {
                    return HTCLIENT;
                }
            }

            return HTCAPTION;
        }

        return HTCLIENT;
    }
    }

    return CallWindowProcW(original_proc, window_handle, message, wparam, lparam);
}

Window* Platform::CreatePlatformWindow(const WindowOptions* Opts) {
    State->PrimaryWindow = (Window*)Malloc(sizeof(Window), false);
    State->PrimaryWindow->Init(Opts);

    State->PrimaryWindow->PlatformWindowState = (PlatformWindowState*)Malloc(sizeof(PlatformWindowState), false);
    MemSet(State->PrimaryWindow->PlatformWindowState, 0, sizeof(PlatformWindowState));
    State->PrimaryWindow->PlatformWindowState->hWindow = glfwGetWin32Window(State->PrimaryWindow->PlatformWindowHandle);
    State->PrimaryWindow->PlatformWindowState->hInstance = GetModuleHandleW(NULL);
    if (Opts->CustomTitleBar) {
        HWND window_handle = State->PrimaryWindow->PlatformWindowState->hWindow;
        SetPropW(window_handle, KRAFT_WINDOW_PROPERTY, State->PrimaryWindow);
        State->PrimaryWindow->PlatformWindowState->OriginalWindowProc = (WNDPROC)SetWindowLongPtrW(window_handle, GWLP_WNDPROC, (LONG_PTR)CustomFrameWindowProc);
        SetWindowPos(window_handle, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        RECT client_rect;
        RECT window_rect;
        GetClientRect(window_handle, &client_rect);
        GetWindowRect(window_handle, &window_rect);
        int client_height_excess = (client_rect.bottom - client_rect.top) - (int)Opts->Height;
        SetWindowPos(window_handle, nullptr, 0, 0, window_rect.right - window_rect.left, (window_rect.bottom - window_rect.top) - client_height_excess, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        glfwGetWindowSize(State->PrimaryWindow->PlatformWindowHandle, &State->PrimaryWindow->Width, &State->PrimaryWindow->Height);
        glfwGetFramebufferSize(State->PrimaryWindow->PlatformWindowHandle, &State->PrimaryWindow->FramebufferWidth, &State->PrimaryWindow->FramebufferHeight);
    }

    return State->PrimaryWindow;
}

void Platform::DestroyPlatformWindow(Window* Window) {
    RemovePropW(State->PrimaryWindow->PlatformWindowState->hWindow, KRAFT_WINDOW_PROPERTY);
    State->PrimaryWindow->Destroy();

    Free(State->PrimaryWindow->PlatformWindowState);
    Free(State->PrimaryWindow);
}

static wchar_t* DialogPushWide(ArenaAllocator* arena, const char* text) {
    int wide_length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    wchar_t* wide = ArenaPushArrayNoZero(arena, wchar_t, (u64)wide_length + 1);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_length);
    wide[wide_length] = 0;

    return wide;
}

static wchar_t* DialogPushFilter(ArenaAllocator* arena, const char* filter) {
    if (!filter || !filter[0])
        return nullptr;

    wchar_t* wide = DialogPushWide(arena, filter);
    for (wchar_t* character = wide; *character; character++) {
        if (*character == L'|')
            *character = 0;
    }

    return wide;
}

static String8 DialogPushUtf8(ArenaAllocator* arena, const wchar_t* wide) {
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

static HWND DialogOwnerWindow() {
    if (Platform::State && Platform::State->PrimaryWindow && Platform::State->PrimaryWindow->PlatformWindowState)
        return Platform::State->PrimaryWindow->PlatformWindowState->hWindow;

    return nullptr;
}

// The common item dialog rather than the old SHBrowseForFolder tree: it is the same picker the
// shell uses everywhere else, so it gets the places bar, typing a path, and a usable size.
bool Platform::OpenFolderDialog(ArenaAllocator* arena, const char* title, String8* out_path) {
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool    uninitialize = initialized == S_OK || initialized == S_FALSE;

    IFileOpenDialog* dialog = nullptr;
    HRESULT          result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                               IID_PPV_ARGS(&dialog));
    if (FAILED(result)) {
        if (uninitialize) {
            CoUninitialize();
        }
        return false;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
    if (title) {
        TempArena scratch = ScratchBegin(&arena, 1);
        i32       length = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        wchar_t*  wide = ArenaPushArray(scratch.arena, wchar_t, length > 0 ? length : 1);
        if (length > 0) {
            MultiByteToWideChar(CP_UTF8, 0, title, -1, wide, length);
            dialog->SetTitle(wide);
        }
        ScratchEnd(scratch);
    }

    bool ok = false;
    if (dialog->Show(DialogOwnerWindow()) == S_OK) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR chosen = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &chosen))) {
                *out_path = DialogPushUtf8(arena, chosen);
                CoTaskMemFree(chosen);
                ok = true;
            }
            item->Release();
        }
    }

    dialog->Release();
    if (uninitialize) {
        CoUninitialize();
    }

    return ok;
}

bool Platform::OpenFileDialog(ArenaAllocator* arena, const char* filter, String8* out_path) {
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

bool Platform::SaveFileDialog(ArenaAllocator* arena, const char* filter, const char* default_extension, String8* out_path) {
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