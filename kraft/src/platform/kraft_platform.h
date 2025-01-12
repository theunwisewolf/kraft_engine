#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct EngineConfig;
enum Keys : uint32;

#if defined(KRAFT_GUI_APP)
struct Window;
struct CreateWindowOptions;
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
#define KRAFT_CONSOLE_VT_RESET_CURSOR         "\x1B[1;1H" // Reset cursor position to upper left
#define KRAFT_CONSOLE_VT_CLEAR_DISPLAY_AFTER  "\x1B[0J"   // Erase in Display (after cursor)
#define KRAFT_CONSOLE_VT_CLEAR_DISPLAY_BEFORE "\x1B[1J"   // Erase in Display (before cursor)
#define KRAFT_CONSOLE_VT_CLEAR_DISPLAY        "\x1B[2J"   // Erase in Display (entire)
#define KRAFT_CONSOLE_VT_CLEAR_LINE_AFTER     "\x1B[0K"   // Erase in Line (after cursor)
#define KRAFT_CONSOLE_VT_CLEAR_LINE_BEFORE    "\x1B[1K"   // Erase in Line (before cursor)
#define KRAFT_CONSOLE_VT_CLEAR_LINE           "\x1B[2K"   // Erase in Line (entire)

// ----------------------------------------------------------------------------
//  Select Graphic Rendition (SGR) sequences
// ----------------------------------------------------------------------------
#define KRAFT_CONSOLE_TEXT_FORMAT_CLEAR     "\x1B[0m" // Clears previously applied formatting
#define KRAFT_CONSOLE_TEXT_FORMAT_BOLD      "\x1B[1m"
#define KRAFT_CONSOLE_TEXT_FORMAT_UNDERLINE "\x1B[4m"

// Text colors
#define KRAFT_CONSOLE_COLOR_BLACK                  "\x1B[30m"
#define KRAFT_CONSOLE_COLOR_RED                    "\x1B[31m"
#define KRAFT_CONSOLE_COLOR_GREEN                  "\x1B[32m"
#define KRAFT_CONSOLE_COLOR_YELLOW                 "\x1B[33m"
#define KRAFT_CONSOLE_COLOR_BLUE                   "\x1B[34m"
#define KRAFT_CONSOLE_COLOR_MAGENTA                "\x1B[35m"
#define KRAFT_CONSOLE_COLOR_CYAN                   "\x1B[36m"
#define KRAFT_CONSOLE_COLOR_WHITE                  "\x1B[37m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_BLACK   "\x1B[90m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_RED     "\x1B[91m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_GREEN   "\x1B[92m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_YELLOW  "\x1B[93m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_BLUE    "\x1B[94m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_MAGENTA "\x1B[95m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_CYAN    "\x1B[96m"
#define KRAFT_CONSOLE_COLOR_HIGH_INTENSITY_WHITE   "\x1B[97m"

// Background colors
#define KRAFT_CONSOLE_BG_COLOR_BLACK                  "\x1B[40m"
#define KRAFT_CONSOLE_BG_COLOR_RED                    "\x1B[41m"
#define KRAFT_CONSOLE_BG_COLOR_GREEN                  "\x1B[42m"
#define KRAFT_CONSOLE_BG_COLOR_YELLOW                 "\x1B[43m"
#define KRAFT_CONSOLE_BG_COLOR_BLUE                   "\x1B[44m"
#define KRAFT_CONSOLE_BG_COLOR_MAGENTA                "\x1B[45m"
#define KRAFT_CONSOLE_BG_COLOR_CYAN                   "\x1B[46m"
#define KRAFT_CONSOLE_BG_COLOR_WHITE                  "\x1B[47m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_BLACK             "\x1B[100m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_RED     "\x1B[101m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_GREEN   "\x1B[102m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_YELLOW  "\x1B[103m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_BLUE    "\x1B[104m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_MAGENTA "\x1B[105m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_CYAN    "\x1B[106m"
#define KRAFT_CONSOLE_BG_COLOR_HIGH_INTENSITY_WHITE   "\x1B[107m"

// 8-bit color / 256 Color
#define KRAFT_CONSOLE_COLOR_256(code)    "\x1B[38;5;" #code "m"
#define KRAFT_CONSOLE_COLOR_BG_256(code) "\x1B[48;5;" #code "m"

// 24-bit true colour
#define KRAFT_CONSOLE_COLOR_RGB(r, g, b)    "\x1B[38;2;" #r ";" #g ";" #b "m"
#define KRAFT_CONSOLE_COLOR_BG_RGB(r, g, b) "\x1B[48;2;" #r ";" #g ";" #b "m"

struct KRAFT_API Platform
{
    // If the platform wants to store any internal state,
    // it should do so in this variable
    static PlatformState* State;

    static bool Init(struct EngineConfig* config);
    static bool PollEvents();
    static void Shutdown();

    // Memory
    static void* Malloc(uint64 size, bool aligned);
    static void* Realloc(void* region, uint64_t size, bool aligned);
    static void  Free(void* region);
    static void* MemZero(void* region, uint64_t size);
    static void* MemCpy(void* dst, const void* src, uint64 size);
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
    // Returns the absolute time in seconds
    static float64 GetAbsoluteTime();

    // Returns the elapsed time in milliseconds
    static float64 GetElapsedTime();

    // Misc
    static void        SleepMilliseconds(uint64_t Milliseconds);
    static const char* GetKeyName(Keys key);
    static const char* GetEnv(const char* Key);
    static bool        ExecuteProcess(const char* WorkingDir, const char* ExecutablePath, const char** Args, char** Output);

#if defined(KRAFT_GUI_APP)
    // Windowing
    static Window* CreatePlatformWindow(const struct CreateWindowOptions* Opts);
    static void    DestroyPlatformWindow(struct Window* Window);
    static Window* GetWindow();
#endif
};

}