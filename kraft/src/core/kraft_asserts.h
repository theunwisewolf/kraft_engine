#pragma once

namespace kraft {

void LogAssert(const char* Expression, const char* Message, const char* File, int Line);

}

#if _MSC_VER
#include <intrin.h>
#define debugBreak() __debugbreak()
#else
#define debugBreak() __builtin_trap()
#endif

// Assert
#define KRAFT_ASSERT(expression)                                                                                                           \
    {                                                                                                                                      \
        if (expression)                                                                                                                    \
        {                                                                                                                                  \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            kraft::LogAssert(#expression, "", __FILE__, __LINE__);                                                                         \
            debugBreak();                                                                                                                  \
        }                                                                                                                                  \
    }

// Assert with a message
#define KRAFT_ASSERT_MESSAGE(expression, message)                                                                                          \
    {                                                                                                                                      \
        if (expression)                                                                                                                    \
        {                                                                                                                                  \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            kraft::LogAssert(#expression, message, __FILE__, __LINE__);                                                                    \
            debugBreak();                                                                                                                  \
        }                                                                                                                                  \
    }

#define KASSERT  KRAFT_ASSERT
#define KASSERTM KRAFT_ASSERT_MESSAGE