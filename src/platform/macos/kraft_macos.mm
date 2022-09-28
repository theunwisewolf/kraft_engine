#include "platform/kraft_platform.h"

#ifdef KRAFT_PLATFORM_MACOSX

#include <unistd.h>
#include <memory>
#include <mach/mach_time.h>

#include "core/kraft_log.h"

namespace kraft
{

void* Platform::InternalState       = nullptr;

// TODO:
int Platform::ConsoleColorRed       = 0;
int Platform::ConsoleColorGreen     = 1;
int Platform::ConsoleColorBlue      = 2;
int Platform::ConsoleColorBGRed     = 3;
int Platform::ConsoleColorBGGreen   = 4;
int Platform::ConsoleColorBGBlue    = 5;

// const char* ColorTable[] = {};

bool Platform::Init()
{
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

void Platform::ConsoleOutputString(const char* str, int color)
{
    fprintf(stdout, "%s", str);
}

void Platform::ConsoleOutputStringError(const char* str, int color)
{
    fprintf(stderr, "%s", str);
}

float64 Platform::GetAbsoluteTime()
{
    return mach_absolute_time();
}

float64 Platform::GetElapsedTime()
{
    return 0.f;
}

void Platform::SleepMilliseconds(uint64_t msec)
{
    if (msec >= 1000) 
    {
        sleep(msec / 1000);
    }

    usleep((msec % 1000) * 1000);
}

}

#endif