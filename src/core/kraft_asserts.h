#pragma once

#include "core/kraft_log.h"
#include "core/kraft_string.h"

namespace kraft
{

inline void LogAssert(const char* expression, const char* message, const char* file, int line)
{
    KFATAL("Assertion failure! Expression: '%s' | Message: '%s' | File: %s | Line: %d", ANSI_TO_TCHAR(expression), ANSI_TO_TCHAR(message), ANSI_TO_TCHAR(file), line);
}

}

#if _MSC_VER
#include <intrin.h>
#define debugBreak() __debugbreak()
#else
#define debugBreak() __builtin_trap()
#endif

// Assert
#define KRAFT_ASSERT(expression)                                        \
    {                                                                   \
        if (expression) {}                                              \
        else                                                            \
        {                                                               \
            kraft::LogAssert(#expression, "", __FILE__, __LINE__);      \
            debugBreak();                                               \
        }                                                               \
    }

// Assert with a message
#define KRAFT_ASSERT_MESSAGE(expression, message)                       \
    {                                                                   \
        if (expression) {}                                              \
        else                                                            \
        {                                                               \
            kraft::LogAssert(#expression, message, __FILE__, __LINE__); \
            debugBreak();                                               \
        }                                                               \
    }

#define KASSERT     KRAFT_ASSERT
#define KASSERTM    KRAFT_ASSERT_MESSAGE