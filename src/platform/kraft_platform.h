#pragma once

#include <cstdint>

#include "core/kraft_core.h"

namespace kraft
{

struct Platform
{
    // If the platform wants to store any internal state,
    // it should do so in this variable
    static void *InternalState;

    static bool Init();
    static void Shutdown();

    // Memory
    static void *Malloc(uint64_t size, bool aligned);
    static void *Realloc(void *region, uint64_t size, bool aligned);
    static void Free(void *region, bool aligned);
    static void *MemZero(void *region, uint64_t size);
    static void *MemCpy(void *dst, void *src, uint64_t size);
    static void *MemSet(void *region, int value, uint64_t size);

    // Console
    static int ConsoleColorRed;
    static int ConsoleColorGreen;
    static int ConsoleColorBlue;
    static int ConsoleColorBGRed;
    static int ConsoleColorBGGreen;
    static int ConsoleColorBGBlue;

    static void ConsoleOutputString(const char *str, int color);
    static void ConsoleOutputStringError(const char *str, int color);

    // Time
    static float GetTime();

    // Misc
    static void SleepMilliseconds(uint64_t msec);
};

}