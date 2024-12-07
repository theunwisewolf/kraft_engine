#pragma once

#include <cstring>
#include <core/kraft_core.h>

namespace kraft
{

// char
KRAFT_INLINE uint64 Strlen(const char* Str)
{
    return strlen(Str);
}

// wchar_t
KRAFT_INLINE uint64 Strlen(const wchar_t* Str)
{
    return wcslen(Str);
}

}