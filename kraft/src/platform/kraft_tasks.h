#pragma once

#include <core/kraft_core.h>
#include <core/kraft_allocators.h>
#include "kraft_threading.h"

namespace kraft {

struct TaskPool;

// A task is handed the arena belonging to the worker that ran it, so results can outlive the call
// with no locking and no copy. It is null when the batch was given no arenas
#define KRAFT_TASK_FUNC(name) void name(ArenaAllocator* arena, u32 worker_index, u64 task_index, void* user_data)
typedef KRAFT_TASK_FUNC(TaskFunc);

struct TaskArenas {
    ArenaAllocator** arenas;
    u32 count;
    u64 first_arena_reserved; // arena 0 also holds this struct and the pointer array
};

struct TaskPoolOptions {
    u32 worker_count = 0; // 0 = one per logical core
    // Per worker, and a thread context holds two of them, so a 24 worker pool commits 23 x 2 x this
    // much up front
    u64 worker_scratch_size = KRAFT_SIZE_MB(2);
};

// Workers block on a semaphore between batches, so an idle pool costs nothing
// Worker 0 is the thread that calls TaskParallelFor, which means a pool of one spawns no threads at all
KRAFT_API TaskPool* TaskPoolCreate(ArenaAllocator* arena, TaskPoolOptions options);
KRAFT_API void TaskPoolDestroy(TaskPool* pool);
KRAFT_API u32 TaskPoolWorkerCount(TaskPool* pool);

KRAFT_API TaskArenas* TaskArenasCreate(TaskPool* pool, u64 size_per_worker);

// One size per worker, for when the caller knows how much will land in each
KRAFT_API TaskArenas* TaskArenasCreateSized(TaskPool* pool, u64* sizes, u32 count);
KRAFT_API void TaskArenasDestroy(TaskArenas* arenas);
KRAFT_API void TaskArenasClear(TaskArenas* arenas);

// Blocking fork-join: returns only once every task has run
// Not re-entrant - a task must not call it again, and it asserts if one does
KRAFT_API void TaskParallelFor(TaskPool* pool, TaskArenas* arenas, u64 task_count, TaskFunc* func, void* user_data);

// Contiguous slice of a larger item count, for when one task should chew a range rather than a
// single item
// `end` is exclusive, and a slice past the end comes back empty
struct TaskRange {
    u64 start;
    u64 end;
};

KRAFT_API TaskRange TaskDivideWork(u64 item_count, u64 slice_count, u64 slice_index);

} // namespace kraft
