#include "kraft_allocators.h"

#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <math/kraft_math.h>

namespace kraft {

ArenaAllocator* CreateArena(ArenaCreateOptions Options)
{
    Options.ChunkSize = kraft::math::AlignUp(Options.ChunkSize, Options.Alignment);
    uint64 ActualSize = sizeof(ArenaAllocator) + Options.ChunkSize;
    uint8* Memory = (uint8*)Malloc(ActualSize, kraft::MEMORY_TAG_NONE, true);
    MemSet(Memory, 0, Options.ChunkSize);

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

void ArenaPop(ArenaAllocator* Arena, uint64 Size)
{
    uint64 AlignedSize = kraft::math::AlignUp(Size, Arena->Options.Alignment);
    Arena->Size = Arena->Size - AlignedSize;

    KDEBUG("Deallocating %d bytes from arena", AlignedSize);
}

char* ArenaPushString(ArenaAllocator* Arena, const char* SrcStr, uint64 LengthInBytes)
{
    char* Dst = (char*)ArenaPush(Arena, LengthInBytes + 1);
    MemCpy(Dst, SrcStr, LengthInBytes);

    Dst[LengthInBytes] = 0;
    return Dst;
}

uint8* ArenaAllocator::Push(uint64 Size, bool Zero)
{
    uint64 AlignedSize = kraft::math::AlignUp(Size, this->Options.Alignment);
    // TODO: Stretch
    KASSERT((AlignedSize + this->Size) <= this->Capacity);

    uint8* AddressIntoTheArena = this->Ptr + this->Size;
    this->Size += AlignedSize;

    if (Zero)
    {
        MemSet(AddressIntoTheArena, 0, Size);
    }

    KDEBUG("Allocating %d bytes from arena", AlignedSize);

    return AddressIntoTheArena;
}

} // namespace kraft