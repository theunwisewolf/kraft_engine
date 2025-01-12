#pragma once

#include <core/kraft_core.h>

#define KRAFT_SIZE_KB(Size) Size * 1024
#define KRAFT_SIZE_MB(Size) KRAFT_SIZE_KB(Size) * 1024

namespace kraft {

enum MemoryTag : int;

struct ArenaCreateOptions
{
    uint64    ChunkSize = 64;
    uint64    Alignment = 64;
    MemoryTag Tag = (MemoryTag)0;
};

// TODO: Arena->Next
struct ArenaAllocator
{
    MemoryTag          Tag;
    uint8*             Ptr;
    uint64             Capacity = 0;
    uint64             Size = 0;
    ArenaCreateOptions Options;

    uint8* Push(uint64 Size, bool Zero);
};

ArenaAllocator* CreateArena(ArenaCreateOptions Options);
void            DestroyArena(ArenaAllocator* Arena);

static inline uint8* ArenaPush(ArenaAllocator* Arena, uint64 Size, bool Zero = false)
{
    return Arena->Push(Size, Zero);
}

void  ArenaPop(ArenaAllocator* Arena, uint64 Size);
char* ArenaPushString(ArenaAllocator* Arena, const char* SrcStr, uint64 Length);

template<typename T>
KRAFT_INLINE T* ArenaPushCArray(ArenaAllocator* Arena, uint32 ElementCount, bool Zero = false)
{
    return (T*)(ArenaPush(Arena, sizeof(T) * ElementCount, Zero));
}

template<typename T>
KRAFT_INLINE void ArenaPopCArray(ArenaAllocator* Arena, T* _, uint32 ElementCount)
{
    ArenaPop(Arena, sizeof(T) * ElementCount);
}

} // namespace kraft