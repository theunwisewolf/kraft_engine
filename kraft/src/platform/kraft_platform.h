#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct EngineConfig;
struct ArenaAllocator;
enum Keys : u32;

#if defined(KRAFT_GUI_APP)
struct Window;
struct WindowOptions;
#endif

struct PlatformState;

// Virtual Terminal Sequences
// ----------------------------------------------------------------------------
//  Operating System Command (OSC) sequences
// ----------------------------------------------------------------------------
#define KRAFT_CONSOLE_VT_SET_TITLE(x) "\x1B]2;" x "\x07" // Set window title to string x

// ----------------------------------------------------------------------------
//  Control Sequence Introducer (CSI) sequences
// ----------------------------------------------------------------------------
#define KRAFT_CONSOLE_VT_RESET_CURSOR "\x1B[1;1H" // Reset cursor position to upper left
#define KRAFT_CONSOLE_VT_CLEAR_DISPLAY_AFTER "\x1B[0J" // Erase in Display (after cursor)
#define KRAFT_CONSOLE_VT_CLEAR_DISPLAY_BEFORE "\x1B[1J" // Erase in Display (before cursor)
#define KRAFT_CONSOLE_VT_CLEAR_DISPLAY "\x1B[2J" // Erase in Display (entire)
#define KRAFT_CONSOLE_VT_CLEAR_LINE_AFTER "\x1B[0K" // Erase in Line (after cursor)
#define KRAFT_CONSOLE_VT_CLEAR_LINE_BEFORE "\x1B[1K" // Erase in Line (before cursor)
#define KRAFT_CONSOLE_VT_CLEAR_LINE "\x1B[2K" // Erase in Line (entire)

// ----------------------------------------------------------------------------
//  Select Graphic Rendition (SGR) sequences
// ----------------------------------------------------------------------------
#define KRAFT_CONSOLE_TEXT_FORMAT_CLEAR "\x1B[0m" // Clears previously applied formatting
#define KRAFT_CONSOLE_TEXT_FORMAT_BOLD "\x1B[1m"
#define KRAFT_CONSOLE_TEXT_FORMAT_UNDERLINE "\x1B[4m"

// Text colors
#define KRAFT_CONSOLE_COLOR_BLACK "\x1B[30m"
#define KRAFT_CONSOLE_COLOR_RED "\x1B[31m"
#define KRAFT_CONSOLE_COLOR_GREEN "\x1B[32m"
#define KRAFT_CONSOLE_COLOR_YELLOW "\x1B[33m"
#define KRAFT_CONSOLE_COLOR_BLUE "\x1B[34m"
#define KRAFT_CONSOLE_COLOR_MAGENTA "\x1B[35m"
#define KRAFT_CONSOLE_COLOR_CYAN "\x1B[36m"
#define KRAFT_CONSOLE_COLOR_WHITE "\x1B[37m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_BLACK "\x1B[90m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_RED "\x1B[91m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_GREEN "\x1B[92m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_YELLOW "\x1B[93m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_BLUE "\x1B[94m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_MAGENTA "\x1B[95m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_CYAN "\x1B[96m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_WHITE "\x1B[97m"

// Background colors
#define KRAFT_CONSOLE_BG_COLOR_BLACK "\x1B[40m"
#define KRAFT_CONSOLE_BG_COLOR_RED "\x1B[41m"
#define KRAFT_CONSOLE_BG_COLOR_GREEN "\x1B[42m"
#define KRAFT_CONSOLE_BG_COLOR_YELLOW "\x1B[43m"
#define KRAFT_CONSOLE_BG_COLOR_BLUE "\x1B[44m"
#define KRAFT_CONSOLE_BG_COLOR_MAGENTA "\x1B[45m"
#define KRAFT_CONSOLE_BG_COLOR_CYAN "\x1B[46m"
#define KRAFT_CONSOLE_BG_COLOR_WHITE "\x1B[47m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_BLACK "\x1B[100m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_RED "\x1B[101m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_GREEN "\x1B[102m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_YELLOW "\x1B[103m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_BLUE "\x1B[104m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_MAGENTA "\x1B[105m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_CYAN "\x1B[106m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_WHITE "\x1B[107m"

// 8-bit color / 256 Color
#define KRAFT_CONSOLE_COLOR_256(code) "\x1B[38;5;" #code "m"
#define KRAFT_CONSOLE_COLOR_BG_256(code) "\x1B[48;5;" #code "m"

// 24-bit true colour
#define KRAFT_CONSOLE_COLOR_RGB(r, g, b) "\x1B[38;2;" #r ";" #g ";" #b "m"
#define KRAFT_CONSOLE_COLOR_BG_RGB(r, g, b) "\x1B[48;2;" #r ";" #g ";" #b "m"

struct KRAFT_API Platform {
    // If the platform wants to store any internal state,
    // it should do so in this variable
    static PlatformState* State;

    static bool Init(struct EngineConfig* config);

    static void SetCrashContext(const char* text);
    static u32 CrashReportCount();
    static void SetCrashReportDirectory(const char* directory);
    static bool PollEvents();
    static void Shutdown();

    // Memory
    static void* MemReserve(u64 size);
    static bool MemCommit(void* address, u64 size);
    static void MemDecommit(void* address, u64 size);
    static void MemRelease(void* address, u64 size);
    static u64 MemPageSize();

    static void* Malloc(u64 size, bool aligned);
    static void* Realloc(void* region, u64 size, bool aligned);
    static void Free(void* region);
    static void* MemZero(void* region, u64 size);
    static void* MemCpy(void* dst, const void* src, u64 size);
    static void* MemSet(void* region, int value, u64 size);
    static int MemCmp(const void* a, const void* b, u64 size);

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
    static const int ConsoleColorLoGray;
    static const int ConsoleColorHiGray;

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

    static void ConsoleSetColor(int Color);
    static void ConsoleOutputString(const char* str);
    static void ConsoleOutputString(const char* str, int color);
    static void ConsoleOutputStringError(const char* str);
    static void ConsoleOutputStringError(const char* str, int color);

    // Resets the console formatting to the original state
    static void ConsoleResetFormatting();

    // Time
    // Returns the unix timestamp in NS
    static u64 TimeNowNS();

    // Returns the clock time in NS
    static f64 GetClockTimeNS();

    // Returns the absolute time in seconds
    static f64 GetAbsoluteTime();

    // Returns the elapsed time in milliseconds
    static f64 GetElapsedTime();

    // Fills Out from the operating system's cryptographic random source;
    // Returns false on failure
    static bool RandomBytes(void* Out, u64 Count);

    // Returns true when `signature` was made over `digest` with the private half of the P-256 key `public_key`
    // `public_key` is the key's x then y and `signature` is r then s, 32 bytes each; `digest` is the 32-byte SHA-256 of what was signed
    static bool VerifyEcdsaP256(const u8* public_key, const u8* digest, const u8* signature);

    // Misc
    static void SleepMilliseconds(u64 Milliseconds);
    static const char* GetKeyName(Keys key);
    static const char* GetEnv(const char* Key);

    // Returns the full path of this program's executable
    static String8 ExecutablePath(ArenaAllocator* arena);
    static String8 ExecutableDirectory(ArenaAllocator* arena);
    static String8 SystemFontDirectory(ArenaAllocator* arena);
    static void InstallCrashHandler();
    static u64 ProcessMemoryBytes();

#if defined(KRAFT_GUI_APP)
    // Windowing
    static Window* CreatePlatformWindow(const struct WindowOptions* Opts);
    static void DestroyPlatformWindow(struct Window* Window);
    static Window* GetWindow();
    static bool OpenFileDialog(ArenaAllocator* arena, const char* filter, String8* out_path); // filter: "JSON Files|*.json|All Files|*.*"

    // `suggested_name` is the file name the dialog starts with
    static bool SaveFileDialog(ArenaAllocator* arena, const char* filter, const char* default_extension, String8* out_path, String8 suggested_name = {});
    static bool OpenFolderDialog(ArenaAllocator* arena, const char* title, String8* out_path);

    // Opens a file, folder or URL the way a double click would: a URL in the default browser, 
    // a file in the app registered for its type
    static bool OpenWithSystem(String8 path);

    // Opens the system's file manager with the file at `path` selected
    static bool RevealInFileManager(String8 path);
#endif
};

} // namespace kraft