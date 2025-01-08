#include "kraft_allocators.h"

#include <core/kraft_asserts.h>
#include <core/kraft_memory.h>
#include <math/kraft_math.h>
#include <core/kraft_log.h>

namespace kraft {

ArenaAllocator* CreateArena(ArenaCreateOptsT Options)
{
    Options.ChunkSize = kraft::math::AlignUp(Options.ChunkSize, Options.Alignment);
    uint64 ActualSize = sizeof(ArenaAllocator) + Options.ChunkSize;
    uint8* Memory = (uint8*)Malloc(ActualSize, kraft::MEMORY_TAG_NONE, true);

    ArenaAllocator* OutArena = (ArenaAllocator*)Memory;
    OutArena->Ptr = Memory + sizeof(ArenaAllocator);
    OutArena->Capacity = Options.ChunkSize;
    OutArena->Size = 0;
    OutArena->Options = Options;

    return OutArena;
}

void DestroyArena(ArenaAllocator* Arena)
{
    Free(Arena, Arena->Capacity, MEMORY_TAG_NONE);
}

uint8* ArenaPush(ArenaAllocator* Arena, uint64 Size, bool Zero)
{
    uint64 AlignedSize = kraft::math::AlignUp(Size, Arena->Options.Alignment);
    // TODO: Stretch
    KASSERT((AlignedSize + Arena->Size) < Arena->Capacity);

    uint8* AddressIntoTheArena = Arena->Ptr + Arena->Size;
    Arena->Size += AlignedSize;

    if (Zero)
    {
        MemSet(AddressIntoTheArena, 0, Size);
    }

    KDEBUG("Allocating %d from arena", AlignedSize);

    return AddressIntoTheArena;
}

void ArenaPop(ArenaAllocator* Arena, uint64 Size)
{
    uint64 AlignedSize = kraft::math::AlignUp(Size, Arena->Options.Alignment);
    Arena->Size = Arena->Size - AlignedSize;

    KDEBUG("Deallocating %d from arena", AlignedSize);
}

char* ArenaPushString(ArenaAllocator* Arena, const char* SrcStr, uint64 LengthInBytes)
{
    char* Dst = (char*)ArenaPush(Arena, LengthInBytes + 1);
    MemCpy(Dst, SrcStr, LengthInBytes);

    Dst[LengthInBytes] = 0;
    return Dst;
}

}