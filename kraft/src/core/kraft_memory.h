#pragma once

#include <core/kraft_core.h>

namespace kraft {

enum MemoryTag
{
    MEMORY_TAG_NONE,
    MEMORY_TAG_STRING,
    MEMORY_TAG_ARRAY,
    MEMORY_TAG_BUFFER,
    MEMORY_TAG_RENDERER,
    MEMORY_TAG_FILE_BUF,
    MEMORY_TAG_TEXTURE,
    MEMORY_TAG_TEXTURE_SYSTEM,
    MEMORY_TAG_MATERIAL_SYSTEM,
    MEMORY_TAG_GEOMETRY_SYSTEM,
    MEMORY_TAG_SHADER_SYSTEM,
    MEMORY_TAG_SHADER_FX,
    MEMORY_TAG_RESOURCE_POOL,

    MEMORY_TAG_NUM_COUNT
};

static const char g_TagStrings[MEMORY_TAG_NUM_COUNT][255] = {
    "UNKNOWN        ", "STRING         ", "ARRAY          ", "BUFFER         ", "RENDERER       ", "FILEBUF        ",
    "TEXTURE        ", "TEXTURE_SYSTEM ", "MATERIAL_SYSTEM", "GEOMETRY_SYSTEM", "SHADER_SYSTEM  ", "SHADER_FX      ",
};

struct MemoryStats
{
    uint64_t Allocated = 0;
    uint64_t AllocationsByTag[MEMORY_TAG_NUM_COUNT] = { 0 };
};

static struct MemoryStats g_MemoryStats;

struct Block
{
    void*     Data;
    size_t    Size;
    MemoryTag Tag;
};

//
// Our custom tiny wrappers over memory allocations
// that will help us track the memory usage of the app
//

KRAFT_API Block MallocBlock(uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE);
KRAFT_API Block ReallocBlock(Block block, uint64_t size);
KRAFT_API void  FreeBlock(Block block);

//
// Normal malloc/free/realloc function wrappers
//
void* Realloc(void* region, uint64 size);
void* ReallocAligned(void* region, uint64 size);

KRAFT_API
void*          Malloc(uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE, bool zero = false);
KRAFT_API void Free(void* region);
KRAFT_API void Free(void* region, uint64 size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE);
KRAFT_API void MemZero(void* region, uint64 size);
KRAFT_API void MemZero(Block block);
KRAFT_API void MemSet(void* region, int value, uint64 size);
KRAFT_API void MemSet(Block block, int value);
KRAFT_API void MemCpy(void* dst, void const* src, uint64 size);
KRAFT_API void MemCpy(Block dst, Block src, uint64 size);
KRAFT_API int  MemCmp(const void* a, const void* b, uint64 size);
KRAFT_API void PrintDebugMemoryInfo();

}