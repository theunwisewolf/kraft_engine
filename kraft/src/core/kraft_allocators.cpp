#include <core/kraft_asserts.h>
#include <core/kraft_memory.h>
#include "kraft_allocators.h"

namespace kraft {

ArenaAllocator* CreateArena(ArenaCreateOptions Options)
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

void DestroyArena(ArenaAllocator* Arena)
{
    Free(Arena, Arena->capacity, MEMORY_TAG_NONE);
}

void ArenaPop(ArenaAllocator* arena, u64 size)
{
    u64 new_position = arena->position - size;
    ArenaPopToPosition(arena, new_position);

    // KDEBUG("Deallocated %d bytes from arena", size);
}

void ArenaPopToPosition(ArenaAllocator* arena, u64 position)
{
    arena->position = position;
    KASSERT(arena->position >= sizeof(ArenaAllocator));
}

String8 ArenaPushString8Empty(ArenaAllocator* arena, u64 size)
{
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, size + 1);
    result.count = size;
    result.ptr[result.count] = 0;

    return result;
}

String8 ArenaPushString8Copy(ArenaAllocator* arena, String8 str)
{
    String8 result = {};
    result.ptr = ArenaPushArrayNoZero(arena, u8, str.count + 1);
    result.count = str.count;
    MemCpy(result.ptr, str.ptr, str.count);
    result.ptr[result.count] = 0;

    return result;
}

void ArenaPopString8(ArenaAllocator* arena, String8 str)
{
    ArenaPop(arena, str.count);
}

char* ArenaPushString(ArenaAllocator* arena, const char* src, u64 length)
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

    u8* address_into_the_arena = this->base_ptr + start_position;
    if (zero)
    {
        MemZero(address_into_the_arena, size);
    }

    // KDEBUG("Allocated %llu bytes from arena", size);
    return address_into_the_arena;
}

u64 ArenaPosition(ArenaAllocator* arena)
{
    return arena->base_position + arena->position;
}

TempArena TempBegin(ArenaAllocator* arena)
{
    return TempArena{ .arena = arena, .position = ArenaPosition(arena) };
}

void TempEnd(TempArena temp_arena)
{
    ArenaPopToPosition(temp_arena.arena, temp_arena.position);
}

} // namespace kraft