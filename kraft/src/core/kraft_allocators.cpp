#include <core/kraft_asserts.h>
#include <core/kraft_memory.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>
#include "kraft_allocators.h"

namespace kraft {

#define KRAFT_ARENA_COMMIT_SIZE KRAFT_SIZE_KB(64)
#define KRAFT_ARENA_DECOMMIT_THRESHOLD KRAFT_SIZE_MB(4)

// A zeroed mutex is an unlocked one on every platform, so the first arena needs nothing set up before it
static Mutex g_live_arenas_lock;
static ArenaAllocator* g_live_arenas;

static u64 ArenaCommitTarget(ArenaAllocator* arena, u64 position) {
    u64 wanted = sizeof(ArenaAllocator) + position;
    u64 target = AlignPow2(wanted, arena->options.CommitSize);

    return target > arena->capacity_reserved ? arena->capacity_reserved : target;
}

static bool ArenaCommitTo(ArenaAllocator* arena, u64 position) {
    u64 target = ArenaCommitTarget(arena, position);
    if (target <= arena->committed) {
        return true;
    }

    u8* reservation = (u8*)arena;
    if (!Platform::MemCommit(reservation + arena->committed, target - arena->committed)) {
        return false;
    }

    MemoryTrackCommit(target - arena->committed, arena->tag);
    arena->committed = target;

    return true;
}

ArenaAllocator* CreateArena(ArenaCreateOptions options) {
    u64 page_size = Platform::MemPageSize();
    if (options.CommitSize == 0) {
        options.CommitSize = KRAFT_ARENA_COMMIT_SIZE;
    }
    if (options.DecommitThreshold == 0) {
        options.DecommitThreshold = KRAFT_ARENA_DECOMMIT_THRESHOLD;
    }
    if (options.CommitSize < page_size) {
        options.CommitSize = page_size;
    }
    options.CommitSize = AlignPow2(options.CommitSize, page_size);
    options.ChunkSize = AlignPow2(options.ChunkSize, options.Alignment);

    u64 reserved_size = AlignPow2(sizeof(ArenaAllocator) + options.ChunkSize, options.CommitSize);
    u8* memory = (u8*)Platform::MemReserve(reserved_size);
    KASSERTM(memory != nullptr, "out of address space");

    // Enough for the arena's own bookkeeping; the rest follows as it is pushed into.
    bool committed = Platform::MemCommit(memory, options.CommitSize);
    KASSERTM(committed, "could not commit the first page of an arena");
    MemoryTrackCommit(options.CommitSize, options.Tag);

    ArenaAllocator* out_arena = (ArenaAllocator*)memory;
    out_arena->tag = options.Tag;
    out_arena->base_ptr = memory + sizeof(ArenaAllocator);
    out_arena->base_position = 0;
    out_arena->options = options;
    out_arena->capacity = options.ChunkSize;
    out_arena->capacity_reserved = reserved_size;
    out_arena->committed = options.CommitSize;
    out_arena->position = sizeof(ArenaAllocator);
    out_arena->high_water = out_arena->position;

    MutexLock(&g_live_arenas_lock);
    out_arena->previous_live = nullptr;
    out_arena->next_live = g_live_arenas;
    if (g_live_arenas) {
        g_live_arenas->previous_live = out_arena;
    }
    g_live_arenas = out_arena;
    MutexUnlock(&g_live_arenas_lock);

    return out_arena;
}

void DestroyArena(ArenaAllocator* arena) {
    MutexLock(&g_live_arenas_lock);
    if (arena->previous_live) {
        arena->previous_live->next_live = arena->next_live;
    } else {
        g_live_arenas = arena->next_live;
    }

    if (arena->next_live) {
        arena->next_live->previous_live = arena->previous_live;
    }

    MutexUnlock(&g_live_arenas_lock);

    MemoryTrackDecommit(arena->committed, arena->tag);
    Platform::MemRelease(arena, arena->capacity_reserved);
}

u64 ArenaResidentBytes(ArenaAllocator* arena) {
    return Platform::MemResidentBytes(arena, arena->committed);
}

u64 ArenasResidentBytes() {
    u64 total = 0;
    MutexLock(&g_live_arenas_lock);
    for (ArenaAllocator* arena = g_live_arenas; arena; arena = arena->next_live) {
        total += ArenaResidentBytes(arena);
    }
    MutexUnlock(&g_live_arenas_lock);

    return total;
}

void DecommitArenaAbove(ArenaAllocator* arena, u64 position) {
    u64 target = ArenaCommitTarget(arena, position);
    if (target >= arena->committed) {
        return;
    }

    u8* reservation = (u8*)arena;
    Platform::MemDecommit(reservation + target, arena->committed - target);
    MemoryTrackDecommit(arena->committed - target, arena->tag);
    arena->committed = target;

    u64 fresh_from = target - sizeof(ArenaAllocator);
    if (arena->high_water > fresh_from) {
        arena->high_water = fresh_from;
    }
}

void ArenaPop(ArenaAllocator* arena, u64 size) {
    u64 new_position = arena->position - size;
    ArenaPopToPosition(arena, new_position);

    // KDEBUG("Deallocated %d bytes from arena", size);
}

void ArenaPopToPosition(ArenaAllocator* arena, u64 position) {
    KASSERT(position >= sizeof(ArenaAllocator));

#if KRAFT_MEMORY_DEBUG
    if (arena->position > position) {
        u64 poison_end = arena->position;
        u64 ceiling = arena->committed - sizeof(ArenaAllocator);
        if (poison_end > ceiling) {
            poison_end = ceiling;
        }
        if (poison_end > position) {
            MemSet(arena->base_ptr + position, 0xfc, poison_end - position);
        }
    }
#endif

    arena->position = position;
    if (arena->committed > sizeof(ArenaAllocator) + position + arena->options.DecommitThreshold) {
        DecommitArenaAbove(arena, position);
    }
}

void ArenaClear(ArenaAllocator* arena) {
    ArenaPopToPosition(arena, sizeof(ArenaAllocator));
}

String8 ArenaPushString8Empty(ArenaAllocator* arena, u64 size) {
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, size + 1);
    result.count = size;
    result.ptr[result.count] = 0;

    return result;
}

String8 ArenaPushString8Copy(ArenaAllocator* arena, String8 str) {
    String8 result = {};
    result.ptr = ArenaPushArrayNoZero(arena, u8, str.count + 1);
    result.count = str.count;
    MemCpy(result.ptr, str.ptr, str.count);
    result.ptr[result.count] = 0;

    return result;
}

void ArenaPopString8(ArenaAllocator* arena, String8 str) {
    ArenaPop(arena, str.count);
}

char* ArenaPushString(ArenaAllocator* arena, const char* src, u64 length) {
    char* dst = (char*)arena->Push(length + 1, AlignOf(char), false);
    MemCpy(dst, src, length);

    dst[length] = 0;
    return dst;
}

void* ArenaAllocator::Push(u64 size, u64 alignment, bool zero) {
    u64 start_position = AlignPow2(this->position, alignment);
    u64 end_position = start_position + size;

    // TODO: Stretch
    KASSERT(end_position <= this->capacity);
    bool committed = ArenaCommitTo(this, end_position);
    KASSERTM(committed, "could not commit arena pages");
    this->position = end_position;

    u8* address_into_the_arena = this->base_ptr + start_position;
    if (zero) {
        u64 dirty_end = end_position < this->high_water ? end_position : this->high_water;
        if (dirty_end > start_position) {
            MemZero(address_into_the_arena, dirty_end - start_position);
        }
    }

    if (end_position > this->high_water) {
        this->high_water = end_position;
    }

    // KDEBUG("Allocated %llu bytes from arena", size);
    return address_into_the_arena;
}

u64 ArenaPosition(ArenaAllocator* arena) {
    return arena->base_position + arena->position;
}

TempArena TempBegin(ArenaAllocator* arena) {
    return TempArena{.arena = arena, .position = ArenaPosition(arena)};
}

void TempEnd(TempArena temp_arena) {
    ArenaPopToPosition(temp_arena.arena, temp_arena.position);
}

} // namespace kraft