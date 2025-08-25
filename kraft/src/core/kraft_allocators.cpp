#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include "kraft_allocators.h"

namespace kraft {

kraft_internal ArenaAllocator* CreateArena(ArenaCreateOptions Options)
{
    Options.ChunkSize = AlignPow2(Options.ChunkSize, Options.Alignment);
    uint64 ActualSize = sizeof(ArenaAllocator) + Options.ChunkSize;
    uint8* Memory = (uint8*)Malloc(ActualSize, kraft::MEMORY_TAG_NONE, true);
    MemSet(Memory, 0, Options.ChunkSize);

    ArenaAllocator* OutArena = (ArenaAllocator*)Memory;
    OutArena->base_ptr = Memory + sizeof(ArenaAllocator);
    OutArena->base_position = 0;
    OutArena->options = Options;
    OutArena->capacity = Options.ChunkSize;
    OutArena->position = sizeof(ArenaAllocator);

    return OutArena;
}

kraft_internal void DestroyArena(ArenaAllocator* Arena)
{
    Free(Arena, Arena->capacity, MEMORY_TAG_NONE);
}

kraft_internal void ArenaPop(ArenaAllocator* arena, u64 size)
{
    u64 new_position = arena->position - size;
    ArenaPopToPosition(arena, new_position);

    // KDEBUG("Deallocated %d bytes from arena", size);
}

kraft_internal void ArenaPopToPosition(ArenaAllocator* arena, u64 position)
{
    arena->position = position;
    KASSERT(arena->position >= sizeof(ArenaAllocator));
}

kraft_internal string8 ArenaPushString8Copy(ArenaAllocator* arena, string8 str)
{
    string8 result = {};
    result.ptr = ArenaPushArrayNoZero(arena, char, str.count);
    result.count = str.count;
    MemCpy(result.ptr, str.ptr, str.count);

    return result;
}

kraft_internal void ArenaPopString8(ArenaAllocator* arena, string8 str)
{
    ArenaPop(arena, str.count);
}

kraft_internal char* ArenaPushString(ArenaAllocator* arena, const char* src, u64 length)
{
    char* dst = (char*)arena->Push(length + 1, AlignOf(char), false);
    MemCpy(dst, src, length);

    dst[length] = 0;
    return dst;
}

void* ArenaAllocator::Push(u64 size, u64 alignment, bool zero)
{
    u64 start_position = AlignPow2(this->position, alignment);
    u64 end_position = start_position + size;

    // TODO: Stretch
    KASSERT(end_position <= this->capacity);
    this->position = end_position;

    uint8* address_into_the_arena = this->base_ptr + start_position;
    if (zero)
    {
        MemZero(address_into_the_arena, size);
    }

    // KDEBUG("Allocated %llu bytes from arena", size);
    return address_into_the_arena;
}

kraft_internal u64 ArenaPosition(ArenaAllocator* arena)
{
    return arena->base_position + arena->position;
}

kraft_internal TempArena TempBegin(ArenaAllocator* arena)
{
    return TempArena{ .arena = arena, .position = ArenaPosition(arena) };
}

kraft_internal void TempEnd(TempArena temp_arena)
{
    ArenaPopToPosition(temp_arena.arena, temp_arena.position);
}

} // namespace kraft