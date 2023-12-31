#pragma once

#include "core/kraft_core.h"
#include "core/kraft_memory.h"

#include <string.h>
#include <ctype.h>

namespace kraft
{

KRAFT_INLINE uint64 StringLength(const char* in)
{
    return strlen(in);
}

KRAFT_INLINE uint64 StringLengthClamped(const char* in, uint64 max)
{
    uint64 length = StringLength(in);
    return length > max ? max : length;
}

KRAFT_INLINE char* StringCopy(char* dst, char* src)
{
    return strcpy(dst, src);
}

KRAFT_INLINE char* StringNCopy(char* dst, const char* src, uint64 length)
{
    return strncpy(dst, src, length);
}

KRAFT_INLINE const char* StringTrim(const char* in)
{
    if (!in) return in;
    
    uint64 length = StringLength(in);
    int start = 0;
    int end = length - 1;
    
    while (start < length && isspace(in[start])) start++;
    while (end >= start && isspace(in[end])) end--;

    length = end - start + 1;
    void* out = kraft::Malloc(length + 1, MEMORY_TAG_STRING, true);
    kraft::MemCpy(out, (void*)(in+start), length);

    return (const char*)out;
}

// Will not work with string-literals as they are read only!
KRAFT_INLINE char* StringTrimLight(char* in)
{
    if (!in) return in;

    while (isspace(*in)) in++;
    char *p = in;

    while (*p) p++;
    p--;

    while (isspace(*p)) p--;
    p[1] = 0;

    return in;
}

KRAFT_INLINE uint64 StringEqual(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

KRAFT_INLINE int32 StringFormat(char* buffer, int n, const char* format, ...)
{
    if (buffer) 
    {
        __builtin_va_list argPtr;
        va_start(argPtr, format);
        int32 written = vsnprintf(buffer, n, format, argPtr);
        va_end(argPtr);
        return written;
    }

    return -1;
}

}