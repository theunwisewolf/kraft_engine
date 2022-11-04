#pragma once

#include "platform/kraft_platform.h"
#include "core/kraft_log.h"

namespace kraft
{

enum MemoryTag
{
    MEMORY_TAG_NONE,
    MEMORY_TAG_ARRAY,
    MEMORY_TAG_RENDERER,
    MEMORY_TAG_FILE_BUF,

    MEMORY_TAG_NUM_COUNT
};

static const char g_TagStrings[MEMORY_TAG_NUM_COUNT][9] = {
    "UNKNOWN ",
    "ARRAY   ",
    "RENDERER",
}; 

struct MemoryStats
{
    uint64_t Allocated                              = 0;
    uint64_t AllocationsByTag[MEMORY_TAG_NUM_COUNT] = {0};
};

static struct MemoryStats g_MemoryStats;

struct Block
{
    void*       Data;
    size_t      Size;
    MemoryTag   Tag;
};

//
// Our custom tiny wrappers over memory allocations
// that will help us track the memory usage of the app
//

KRAFT_API Block MallocBlock(uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE);
KRAFT_API Block ReallocBlock(Block block, uint64_t size);
KRAFT_API void FreeBlock(Block block);

//
// Normal malloc/free/realloc function wrappers
//

KRAFT_API void* Malloc(uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE, bool zero = false);
KRAFT_INLINE void* Realloc(void *region, uint64_t size)
{
    return Platform::Realloc(region, size, false);
}

KRAFT_INLINE void Free(void *region)
{
    Platform::Free(region, false);
}

KRAFT_API void Free(void *region, uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE);

KRAFT_INLINE void MemZero(void *region, uint64_t size)
{
    Platform::MemZero(region, size);
}

KRAFT_INLINE void MemZero(Block block)
{
    Platform::MemZero(block.Data, block.Size);
}

KRAFT_INLINE void MemSet(void *region, int value, uint64_t size)
{
    Platform::MemSet(region, value, size);
}

KRAFT_INLINE void MemSet(Block block, int value)
{
    MemSet(block.Data, value, block.Size);
}

KRAFT_INLINE void MemCpy(void *dst, void *src, size_t size)
{
    Platform::MemCpy(dst, src, size);
}

KRAFT_INLINE void MemCpy(Block dst, Block src, size_t size)
{
    MemCpy(dst.Data, src.Data, size);
}

KRAFT_API void PrintDebugMemoryInfo();

}