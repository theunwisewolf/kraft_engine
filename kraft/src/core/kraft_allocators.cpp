#include "kraft_allocators.h"

#include <core/kraft_asserts.h>
#include <core/kraft_memory.h>
#include <math/kraft_math.h>

namespace kraft {

ArenaT* CreateArena(ArenaCreateOptsT Options)
{
    Options.ChunkSize = kraft::math::AlignUp(Options.ChunkSize, Options.Alignment);
    uint64 ActualSize = sizeof(ArenaT) + Options.ChunkSize;
    uint8* Memory = (uint8*)Malloc(ActualSize, kraft::MEMORY_TAG_NONE, true);

    ArenaT* OutArena = (ArenaT*)Memory;
    OutArena->Ptr = Memory + sizeof(ArenaT);
    OutArena->Capacity = Options.ChunkSize;
    OutArena->Size = 0;
    OutArena->Options = Options;

    return OutArena;
}

void DestroyArena(ArenaT* Arena)
{
    Free(Arena, Arena->Capacity, MEMORY_TAG_NONE);
}

uint8* ArenaPush(ArenaT* Arena, uint64 Size)
{
    uint64 AlignedSize = kraft::math::AlignUp(Size, Arena->Options.Alignment);
    // TODO: Stretch
    KASSERT((AlignedSize + Arena->Size) < Arena->Capacity);

    Arena->Ptr += AlignedSize;
    Arena->Size += AlignedSize;

    return Arena->Ptr;
}

char* ArenaPushString(ArenaT* Arena, const char* SrcStr, uint64 LengthInBytes)
{
    char* Dst = (char*)ArenaPush(Arena, LengthInBytes + 1);
    MemCpy(Dst, SrcStr, LengthInBytes);

    Dst[LengthInBytes] = 0;
    return Dst;
}

}