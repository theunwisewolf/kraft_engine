namespace kraft {

struct MemoryStats g_MemoryStats;

MemoryStats::~MemoryStats()
{
    MemoryStatsOutput CurrentMemoryStats = GetMemoryStats();
    if (CurrentMemoryStats.AllocatedBytes > 0)
    {
        KWARN("[Engine]: %.3f KiB memory still allocated", CurrentMemoryStats.AllocatedKiB);
        PrintDebugMemoryInfo();
    }
}

KRAFT_API MemoryBlock MallocBlock(uint64_t size, MemoryTag tag, bool Zero)
{
    g_MemoryStats.Allocated += size;
    g_MemoryStats.AllocationsByTag[tag] += size;

    void* region = Platform::Malloc(size, false);
    if (Zero)
    {
        MemZero(region, size);
    }

    return { (uint8*)region, size, tag };
}

KRAFT_API MemoryBlock ReallocBlock(MemoryBlock block, uint64_t size)
{
    g_MemoryStats.Allocated += size - block.Size;
    g_MemoryStats.AllocationsByTag[block.Tag] += size - block.Size;

    void* region = Platform::Realloc(block.Data, size, false);
    block.Data = (uint8*)region;
    block.Size = size;

    return block;
}

KRAFT_API void FreeBlock(MemoryBlock block)
{
    g_MemoryStats.Allocated -= block.Size;
    g_MemoryStats.AllocationsByTag[block.Tag] -= block.Size;

    Platform::Free(block.Data);
}

KRAFT_API void* Malloc(uint64_t size)
{
    return Platform::Malloc(size, false);
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

KRAFT_API void* Realloc(void* region, uint64 size, MemoryTag tag)
{
    g_MemoryStats.Allocated += size;
    g_MemoryStats.AllocationsByTag[tag] += size;

    return Platform::Realloc(region, size, false);
}

KRAFT_API void* Realloc(void* region, uint64 size)
{
    return Platform::Realloc(region, size, false);
}

KRAFT_API void Free(void* region)
{
    Platform::Free(region);
}

KRAFT_API void Free(void* region, uint64_t size, MemoryTag tag)
{
    g_MemoryStats.Allocated -= size;
    g_MemoryStats.AllocationsByTag[tag] -= size;

    Platform::Free(region);
}

KRAFT_API void MemZero(void* region, uint64 size)
{
    Platform::MemZero(region, size);
}

KRAFT_API void MemZero(MemoryBlock block)
{
    Platform::MemZero(block.Data, block.Size);
}

KRAFT_API void MemSet(void* region, int value, uint64 size)
{
    Platform::MemSet(region, value, size);
}

KRAFT_API void MemSet(MemoryBlock block, int value)
{
    MemSet(block.Data, value, block.Size);
}

KRAFT_API void MemCpy(void* dst, void const* src, uint64 size)
{
    Platform::MemCpy(dst, src, size);
}

KRAFT_API void MemCpy(MemoryBlock dst, MemoryBlock src, uint64 size)
{
    MemCpy(dst.Data, src.Data, size);
}

KRAFT_API int MemCmp(const void* a, const void* b, uint64 size)
{
    return Platform::MemCmp(a, b, size);
}

KRAFT_API void PrintDebugMemoryInfo()
{
    const size_t kib = 1024;
    const size_t mib = kib * kib;
    const size_t gib = mib * kib;

    if (g_MemoryStats.Allocated > gib)
    {
        KINFO("Total memory usage - %.3f GB", g_MemoryStats.Allocated / (float)gib);
    }
    else if (g_MemoryStats.Allocated > mib)
    {
        KINFO("Total memory usage - %.3f MB", g_MemoryStats.Allocated / (float)mib);
    }
    else
    {
        KINFO("Total memory usage - %.3f KB", g_MemoryStats.Allocated / (float)kib);
    }

    KINFO("Memory Stats: ");
    for (int i = 0; i < MemoryTag::MEMORY_TAG_NUM_COUNT; i++)
    {
        KINFO("\t[%s]: %d bytes", g_TagStrings[i], g_MemoryStats.AllocationsByTag[i]);
    }
}

KRAFT_API MemoryStatsOutput GetMemoryStats()
{
    MemoryStatsOutput Output;

    const size_t kib = 1024;
    const size_t mib = kib * kib;
    const size_t gib = mib * kib;

    Output.AllocatedBytes = g_MemoryStats.Allocated;
    Output.AllocatedKiB = g_MemoryStats.Allocated / (float)kib;
    Output.AllocatedMiB = g_MemoryStats.Allocated / (float)mib;
    Output.AllocatedGiB = g_MemoryStats.Allocated / (float)gib;

    return Output;
}
}