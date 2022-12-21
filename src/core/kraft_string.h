#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"

#include <string.h>

namespace kraft
{

KRAFT_INLINE uint64 StringLength(const char* in)
{
    return strlen(in);
}

KRAFT_INLINE uint64 StringLengthClamped(const char* in, uint64 max)
{
    uint64 length = strlen(in);
    return length > max ? max : length;
}

KRAFT_INLINE void StringCopy(char* dst, uint64 size, char* src)
{
    return MemCpy(dst, src, size);
}

}