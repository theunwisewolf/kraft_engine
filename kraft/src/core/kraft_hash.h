#pragma once

#include <stddef.h>

#include <core/kraft_core.h>

namespace kraft {

// https://learn.microsoft.com/en-us/cpp/porting/fix-your-dependencies-on-library-internals?view=msvc-170
KRAFT_INLINE static size_t FNV1AHashBytes(const unsigned char* Buffer, size_t Count)
{
#if defined(KRAFT_64BIT)
    static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
    const size_t fnv_offset_basis = 14695981039346656037ULL;
    const size_t fnv_prime = 1099511628211ULL;
#else  /* defined(_WIN64) */
    static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
    const size_t fnv_offset_basis = 2166136261U;
    const size_t fnv_prime = 16777619U;
#endif /* defined(_WIN64) */

    size_t result = fnv_offset_basis;
    for (size_t next = 0; next < Count; ++next)
    {
        // fold in another byte
        result ^= (size_t)Buffer[next];
        result *= fnv_prime;
    }

    return result;
}

// Credit:
// http://bitsquid.blogspot.com/2011/08/code-snippet-murmur-hash-inverse-pre.html
uint32 MurmurHash(const void* key, int len, uint32 seed);
uint32 MurmurHashInverse(uint32 h, uint32 seed);
uint64 MurmurHash64(const void* key, int len, uint64 seed);
uint64 MurmurHash64Inverse(uint64 h, uint64 seed);

}