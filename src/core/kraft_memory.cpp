#include "kraft_memory.h"

namespace kraft
{

KRAFT_API Block MallocBlock(uint64_t size, MemoryTag tag)
{
    g_MemoryStats.Allocated += size;
    g_MemoryStats.AllocationsByTag[tag] += size;
    
    void *region = Platform::Malloc(size, false);
    return {region, size, tag};
}

KRAFT_API Block ReallocBlock(Block block, uint64_t size)
{
    g_MemoryStats.Allocated += size - block.Size;
    g_MemoryStats.AllocationsByTag[block.Tag] += size - block.Size;
    
    void *region = Platform::Realloc(block.Data, size, false);
    block.Data = region;
    block.Size = size;
    
    return block;
}

KRAFT_API void FreeBlock(Block block)
{
    g_MemoryStats.Allocated -= block.Size;
    g_MemoryStats.AllocationsByTag[block.Tag] -= block.Size;

    Platform::Free(block.Data, false);
}

KRAFT_API void* Malloc(uint64_t size, MemoryTag tag, bool zero)
{
    g_MemoryStats.Allocated += size;
    g_MemoryStats.AllocationsByTag[tag] += size;

    void* retval = Platform::Malloc(size, false);
    if (zero)
    {
        MemZero(retval, size);
    }

    return retval;
}

KRAFT_API void Free(void *region, uint64_t size, MemoryTag tag)
{
    g_MemoryStats.Allocated -= size;
    g_MemoryStats.AllocationsByTag[tag] -= size;

    Platform::Free(region, false);
}

KRAFT_API void PrintDebugMemoryInfo()
{
    const size_t kib = 1024;
    const size_t mib = kib * kib;
    const size_t gib = mib * kib;

    if (g_MemoryStats.Allocated > gib)
    {
        KINFO(TEXT("Total memory usage - %d GB"), g_MemoryStats.Allocated / (float) gib);
    }
    else if (g_MemoryStats.Allocated > mib)
    {
        KINFO(TEXT("Total memory usage - %d MB"), g_MemoryStats.Allocated / (float) mib);
    }
    else
    {
        KINFO(TEXT("Total memory usage - %d KB"), g_MemoryStats.Allocated / (float) kib);
    }

    // for (int i = 0; i < MemoryTag::MEMORY_TAG_NUM_COUNT; i++)
    // {

    // }
}

}