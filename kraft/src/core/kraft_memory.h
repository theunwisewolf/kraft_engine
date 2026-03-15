#pragma once

namespace kraft {

enum MemoryTag : int
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
    MEMORY_TAG_ASSET_DB,

    MEMORY_TAG_NUM_COUNT
};

static const char g_TagStrings[MEMORY_TAG_NUM_COUNT][255] = { "UNKNOWN        ", "STRING         ", "ARRAY          ", "BUFFER         ", "RENDERER       ", "FILE_BUFFER    ", "TEXTURE        ",
                                                              "TEXTURE_SYSTEM ", "MATERIAL_SYSTEM", "GEOMETRY_SYSTEM", "SHADER_SYSTEM  ", "SHADER_FX      ", "RESOURCE_POOL  ", "ASSET_DATABASE " };

struct MemoryStats
{
    u64 Allocated = 0;
    u64 AllocationsByTag[MEMORY_TAG_NUM_COUNT] = { 0 };

    ~MemoryStats();
};

extern struct MemoryStats g_MemoryStats;

struct MemoryBlock
{
    u8*       Data;
    size_t    Size;
    MemoryTag Tag;
};

struct MemoryStatsOutput
{
    f64 AllocatedBytes = 0;
    f64 AllocatedKiB = 0;
    f64 AllocatedMiB = 0;
    f64 AllocatedGiB = 0;
};

//
// Our custom tiny wrappers over memory allocations
// that will help us track the memory usage of the app
//

KRAFT_API MemoryBlock MallocBlock(u64 size, MemoryTag tag = MemoryTag::MEMORY_TAG_NONE, bool zero = false);
KRAFT_API MemoryBlock ReallocBlock(MemoryBlock block, u64 size);
KRAFT_API void        FreeBlock(MemoryBlock block);

//
// Normal malloc/free/realloc function wrappers
//
KRAFT_API void* Realloc(void* region, u64 size, MemoryTag tag);
KRAFT_API void* Realloc(void* region, u64 size);

KRAFT_API
void*                       Malloc(uint64_t size);
void*                       Malloc(uint64_t size, MemoryTag tag, bool zero = false);
KRAFT_API void              Free(void* region);
KRAFT_API void              Free(void* region, u64 size, MemoryTag tag);
KRAFT_API void              MemZero(void* region, u64 size);
KRAFT_API void              MemZero(MemoryBlock block);
KRAFT_API void              MemSet(void* region, int value, u64 size);
KRAFT_API void              MemSet(MemoryBlock block, int value);
KRAFT_API void              MemCpy(void* dst, void const* src, u64 size);
KRAFT_API void              MemCpy(MemoryBlock dst, MemoryBlock src, u64 size);
KRAFT_API int               MemCmp(const void* a, const void* b, u64 size);
KRAFT_API void              PrintDebugMemoryInfo();
KRAFT_API MemoryStatsOutput GetMemoryStats();

} // namespace kraft