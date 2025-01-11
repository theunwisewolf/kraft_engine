#include "kraft_linux.h"

#include <core/kraft_core.h>

#ifdef KRAFT_PLATFORM_LINUX

#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <core/kraft_log.h>
#include <core/kraft_engine.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_window.h>
#include "kraft_linux_types.h"

namespace kraft
{

LinuxPlatformState* State                 = nullptr;
void* Platform::InternalState             = nullptr;

const int Platform::ConsoleColorBlack     = 30;
const int Platform::ConsoleColorLoWhite   = 37;
const int Platform::ConsoleColorHiWhite   = 97;
const int Platform::ConsoleColorLoRed     = 31;
const int Platform::ConsoleColorHiRed     = 91;
const int Platform::ConsoleColorLoGreen   = 32;
const int Platform::ConsoleColorHiGreen   = 92;
const int Platform::ConsoleColorLoBlue    = 34;
const int Platform::ConsoleColorHiBlue    = 94;
const int Platform::ConsoleColorLoYellow  = 33;
const int Platform::ConsoleColorHiYellow  = 93;
const int Platform::ConsoleColorLoCyan    = 36;
const int Platform::ConsoleColorHiCyan    = 96;
const int Platform::ConsoleColorLoMagenta = 35;
const int Platform::ConsoleColorHiMagenta = 95;

const int Platform::ConsoleColorBGBlack     = 40    << 16;
const int Platform::ConsoleColorBGLoWhite   = 47    << 16;
const int Platform::ConsoleColorBGHiWhite   = 107   << 16;
const int Platform::ConsoleColorBGLoRed     = 41    << 16;
const int Platform::ConsoleColorBGHiRed     = 101   << 16;
const int Platform::ConsoleColorBGLoGreen   = 42    << 16;
const int Platform::ConsoleColorBGHiGreen   = 102   << 16;
const int Platform::ConsoleColorBGLoBlue    = 44    << 16;
const int Platform::ConsoleColorBGHiBlue    = 104   << 16;
const int Platform::ConsoleColorBGLoYellow  = 43    << 16;
const int Platform::ConsoleColorBGHiYellow  = 103   << 16;
const int Platform::ConsoleColorBGLoCyan    = 46    << 16;
const int Platform::ConsoleColorBGHiCyan    = 106   << 16;
const int Platform::ConsoleColorBGLoMagenta = 45    << 16;
const int Platform::ConsoleColorBGHiMagenta = 105   << 16;

bool Platform::Init(struct EngineConfig* config)
{
    InternalState = Malloc(sizeof(LinuxPlatformState), false);
    State = (LinuxPlatformState*)InternalState;
    State->Window.Init(config->WindowTitle, config->WindowWidth, config->WindowHeight, config->RendererBackend);

    return true;
}

bool Platform::PollEvents()
{
    return State->Window.PollEvents();
}

void Platform::Shutdown()
{

}

// ------------------------------------------ 
// Memory Specific Functions
// ------------------------------------------

void *Platform::Malloc(uint64 size, bool aligned)
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

void *Platform::MemCpy(void* Dst, const void* Src, uint64 Size)
{
    return memcpy(Dst, Src, Size);
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

void Platform::ConsoleOutputString(const char* str, int color)
{
    // \033 is the control code
    //"\033[{FORMAT_ATTRIBUTE};{FORGROUND_COLOR};{BACKGROUND_COLOR}m{TEXT}\033[{RESET_FORMATE_ATTRIBUTE}m"
    const int formatAttribute = 0; // For bold, italics and other fancy stuff
    int foregroundColor = color & 0xffff;
    int backgroundColor = color >> 0x10; 
    const int resetFormatAttribute = 0;
    fprintf(stdout, "\033[%d;%d;%dm%s\033[%dm", formatAttribute, foregroundColor, backgroundColor == 0 ? 49 : backgroundColor, str, resetFormatAttribute);
}

void Platform::ConsoleOutputStringError(const char* str, int color)
{
    const int formatAttribute = 0; // For bold, italics and other fancy stuff
    int foregroundColor = color & 0xffff;
    int backgroundColor = color >> 0x10; 
    const int resetFormatAttribute = 0;
    fprintf(stderr, "\033[%d;%d;%dm%s\033[%dm", formatAttribute, foregroundColor, backgroundColor == 0 ? 49 : backgroundColor, str, resetFormatAttribute);
}

float64 Platform::GetAbsoluteTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec + (ts.tv_nsec / 1000000000.f);
}

float64 Platform::GetElapsedTime()
{
    return 0.f;
}

void Platform::SleepMilliseconds(uint64_t Milliseconds)
{
    if (Milliseconds >= 1000) 
    {
        sleep(Milliseconds / 1000);
    }

    usleep((Milliseconds % 1000) * 1000);
}

const char* Platform::GetKeyName(Keys key)
{
    int keycode = (int)key;
    return glfwGetKeyName(keycode, glfwGetKeyScancode(keycode));
}

const char* Platform::GetEnv(const char* Key)
{
    return getenv(Key);
}

}

#endif