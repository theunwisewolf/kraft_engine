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

static Block MallocBlock(uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE)
{
    g_MemoryStats.Allocated += size;
    g_MemoryStats.AllocationsByTag[tag] += size;
    
    void *region = Platform::Malloc(size, false);
    return {region, size, tag};
}

static Block ReallocBlock(Block block, uint64_t size)
{
    g_MemoryStats.Allocated += size - block.Size;
    g_MemoryStats.AllocationsByTag[block.Tag] += size - block.Size;
    
    void *region = Platform::Realloc(block.Data, size, false);
    block.Data = region;
    block.Size = size;
    
    return block;
}

static void FreeBlock(Block block)
{
    g_MemoryStats.Allocated -= block.Size;
    g_MemoryStats.AllocationsByTag[block.Tag] -= block.Size;

    Platform::Free(block.Data, false);
}

//
// Normal malloc/free/realloc function wrappers
//

static void* Malloc(uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE)
{
    g_MemoryStats.Allocated += size;
    g_MemoryStats.AllocationsByTag[tag] += size;

    return Platform::Malloc(size, false);
}

static void* Realloc(void *region, uint64_t size)
{   
    return Platform::Realloc(region, size, false);
}

static void Free(void *region)
{
    Platform::Free(region, false);
}

static void Free(void *region, uint64_t size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE)
{
    g_MemoryStats.Allocated -= size;
    g_MemoryStats.AllocationsByTag[tag] -= size;

    Platform::Free(region, false);
}

static void ZeroMemory(void *region, uint64_t size)
{
    Platform::MemZero(region, size);
}

static void ZeroMemory(Block block)
{
    Platform::MemZero(block.Data, block.Size);
}

static void MemSet(void *region, int value, uint64_t size)
{
    Platform::MemSet(region, value, size);
}

static void MemSet(Block block, int value)
{
    MemSet(block.Data, value, block.Size);
}

static void MemCpy(void *dst, void *src, size_t size)
{
    Platform::MemCpy(dst, src, size);
}

static void MemCpy(Block dst, Block src, size_t size)
{
    MemCpy(dst.Data, src.Data, size);
}

static void PrintDebugMemoryInfo()
{
    const size_t kib = 1024;
    const size_t mib = kib * kib;
    const size_t gib = mib * kib;

    if (g_MemoryStats.Allocated > gib)
    {
        KINFO("Total memory usage - %d GB", g_MemoryStats.Allocated / (float) gib);
    }
    else if (g_MemoryStats.Allocated > mib)
    {
        KINFO("Total memory usage - %d MB", g_MemoryStats.Allocated / (float) mib);
    }
    else
    {
        KINFO("Total memory usage - %d KB", g_MemoryStats.Allocated / (float) kib);
    }

    // for (int i = 0; i < MemoryTag::MEMORY_TAG_NUM_COUNT; i++)
    // {

    // }
}

}