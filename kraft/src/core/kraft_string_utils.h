#pragma once

#include <cstring>

#include <core/kraft_core.h>

namespace kraft {

KRAFT_INLINE static u64 Strlen(const char* str)
{
    return strlen(str);
}

} // namespace kraft