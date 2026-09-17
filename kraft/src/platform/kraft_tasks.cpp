#include "kraft_tasks.h"

#include <core/kraft_core_includes.h>

namespace kraft {

struct TaskWorker {
    u32 index;
    TaskPool* pool;
    Thread thread;
};

struct TaskPool {
    volatile i32 live;
    Semaphore work_ready;
    Semaphore work_done;
    u32 worker_count;
    u64 worker_scratch_size;
    TaskWorker* workers;

    TaskArenas* batch_arenas;
    TaskFunc* batch_func;
    void* batch_user_data;
    u64 batch_task_count;
    volatile i64 tasks_left;
    volatile i64 tasks_done;
    volatile i64 workers_running;
};

// One shared counter, decremented once per task
// Whoever gets there first runs it, so a batch of uneven tasks balances itself and there is nothing to steal
kraft_internal void TaskPoolRunBatch(TaskPool* pool, u32 worker_index) {
    for (;;) {
        i64 remaining = AtomicDecrement64(&pool->tasks_left);
        if (remaining < 0) {
            break;
        }

        u64 task_index = pool->batch_task_count - (u64)(remaining + 1);
        ArenaAllocator* arena = pool->batch_arenas ? pool->batch_arenas->arenas[worker_index] : nullptr;
        pool->batch_func(arena, worker_index, task_index, pool->batch_user_data);
        AtomicIncrement64(&pool->tasks_done);
    }

    // Exactly worker_count threads reach this per batch, so exactly one sees zero.
    if (AtomicDecrement64(&pool->workers_running) == 0) {
        SemaphoreDrop(pool->work_done, 1);
    }
}

kraft_internal KRAFT_THREAD_ENTRY(TaskWorkerMain) {
    TaskWorker* worker = (TaskWorker*)user_data;
    TaskPool* pool = worker->pool;

    // Scratch arenas are thread local, so a worker without its own context would hand out the
    // launching thread's scratch and corrupt it the moment two tasks used ScratchBegin
    ThreadContext* context = CreateThreadContext(pool->worker_scratch_size);
    SetCurrentThreadContext(context);

    for (;;) {
        SemaphoreTake(pool->work_ready, KRAFT_WAIT_FOREVER);
        if (!AtomicLoad32(&pool->live))
            break;

        TaskPoolRunBatch(pool, worker->index);
    }

    SetCurrentThreadContext(nullptr);
    DestroyThreadContext(context);
}

TaskPool* TaskPoolCreate(ArenaAllocator* arena, TaskPoolOptions options) {
    u32 worker_count = options.worker_count > 0 ? options.worker_count : LogicalCoreCount();
    if (worker_count == 0) {
        worker_count = 1;
    }

    TaskPool* pool = ArenaPush(arena, TaskPool);
    pool->live = 1;
    pool->worker_count = worker_count;
    pool->worker_scratch_size = options.worker_scratch_size > 0 ? options.worker_scratch_size : KRAFT_SIZE_MB(2);
    pool->workers = ArenaPushArray(arena, TaskWorker, worker_count);
    for (u32 index = 0; index < worker_count; index++) {
        pool->workers[index].index = index;
        pool->workers[index].pool = pool;
    }

    if (worker_count > 1) {
        pool->work_ready = SemaphoreCreate(0, worker_count);
        pool->work_done = SemaphoreCreate(0, worker_count);

        for (u32 index = 1; index < worker_count; index++) {
            String8 name = StringFormat(arena, "kraft worker %u", index);
            pool->workers[index].thread = ThreadLaunch(TaskWorkerMain, &pool->workers[index], name);
        }
    }

    return pool;
}

void TaskPoolDestroy(TaskPool* pool) {
    if (!pool) {
        return;
    }

    AtomicStore32(&pool->live, 0);
    if (pool->worker_count > 1) {
        SemaphoreDrop(pool->work_ready, pool->worker_count - 1);
        for (u32 index = 1; index < pool->worker_count; index++)
            ThreadJoin(pool->workers[index].thread);

        SemaphoreDestroy(pool->work_ready);
        SemaphoreDestroy(pool->work_done);
    }

    pool->work_ready = Semaphore{};
    pool->work_done = Semaphore{};
    pool->worker_count = 0;
}

u32 TaskPoolWorkerCount(TaskPool* pool) {
    return pool ? pool->worker_count : 0;
}

// The array of arenas lives inside the first one, so the whole set is one bookkeeping unit
TaskArenas* TaskArenasCreate(TaskPool* pool, u64 size_per_worker) {
    u32 count = TaskPoolWorkerCount(pool);
    KASSERTM(count > 0, "TaskArenasCreate needs a live pool");

    TempArena scratch = ScratchBegin(nullptr, 0);
    u64* sizes = ArenaPushArray(scratch.arena, u64, count);
    for (u32 index = 0; index < count; index++) {
        sizes[index] = size_per_worker;
    }

    TaskArenas* result = TaskArenasCreateSized(pool, sizes, count);
    ScratchEnd(scratch);

    return result;
}

TaskArenas* TaskArenasCreateSized(TaskPool* pool, u64* sizes, u32 count) {
    KASSERTM(count > 0 && count == TaskPoolWorkerCount(pool), "TaskArenasCreateSized wants one size per worker");

    TempArena scratch = ScratchBegin(nullptr, 0);
    ArenaAllocator** created = ArenaPushArray(scratch.arena, ArenaAllocator*, count);
    for (u32 index = 0; index < count; index++) {
        ArenaCreateOptions options = {};
        options.ChunkSize = sizes[index] > KRAFT_SIZE_KB(64) ? sizes[index] : KRAFT_SIZE_KB(64);
        options.Alignment = KRAFT_SIZE_KB(64);
        created[index] = CreateArena(options);
    }

    TaskArenas* result = ArenaPush(created[0], TaskArenas);
    ArenaAllocator** arenas = ArenaPushArray(created[0], ArenaAllocator*, count);
    for (u32 index = 0; index < count; index++) {
        arenas[index] = created[index];
    }

    result->arenas = arenas;
    result->count = count;
    result->first_arena_reserved = created[0]->position;
    ScratchEnd(scratch);

    return result;
}

void TaskArenasDestroy(TaskArenas* arenas) {
    if (!arenas)
        return;

    // Arena 0 holds the TaskArenas struct itself, so it has to go last
    ArenaAllocator* first = arenas->arenas[0];
    for (u32 index = arenas->count; index > 1; index--)
        DestroyArena(arenas->arenas[index - 1]);

    DestroyArena(first);
}

void TaskArenasClear(TaskArenas* arenas) {
    if (!arenas)
        return;

    for (u32 index = 1; index < arenas->count; index++)
        ArenaClear(arenas->arenas[index]);

    // Arena 0 keeps the struct and the pointer array, so it rewinds to just past them
    ArenaPopToPosition(arenas->arenas[0], arenas->first_arena_reserved);
}

void TaskParallelFor(TaskPool* pool, TaskArenas* arenas, u64 task_count, TaskFunc* func, void* user_data) {
    if (task_count == 0)
        return;

    KASSERTM(pool && pool->worker_count > 0, "TaskParallelFor needs a live pool");
    KASSERTM(!arenas || arenas->count >= pool->worker_count, "a batch needs one arena per worker");

    // One task, or nobody to hand work to, runs right here with no signalling at all
    if (task_count == 1 || pool->worker_count == 1) {
        ArenaAllocator* arena = arenas ? arenas->arenas[0] : nullptr;
        for (u64 task_index = 0; task_index < task_count; task_index++) {
            func(arena, 0, task_index, user_data);
        }

        return;
    }

    KASSERTM(AtomicLoad64(&pool->tasks_left) <= 0, "TaskParallelFor is not re-entrant");

    pool->batch_arenas = arenas;
    pool->batch_func = func;
    pool->batch_user_data = user_data;
    pool->batch_task_count = task_count;
    AtomicStore64(&pool->tasks_done, 0);
    AtomicStore64(&pool->workers_running, (i64)pool->worker_count);
    // Published last: a worker that wakes early must not see a count before the rest of the batch
    AtomicStore64(&pool->tasks_left, (i64)task_count);

    SemaphoreDrop(pool->work_ready, pool->worker_count - 1);
    TaskPoolRunBatch(pool, 0);
    SemaphoreTake(pool->work_done, KRAFT_WAIT_FOREVER);

    KASSERTM((u64)AtomicLoad64(&pool->tasks_done) == task_count, "a batch finished without running every task");
}

TaskRange TaskDivideWork(u64 item_count, u64 slice_count, u64 slice_index) {
    TaskRange range = {};
    if (slice_count == 0)
        return range;

    u64 per_slice = (item_count + slice_count - 1) / slice_count;
    u64 start = slice_index * per_slice;
    range.start = start < item_count ? start : item_count;
    u64 end = start + per_slice;
    range.end = end < item_count ? end : item_count;

    return range;
}

} // namespace kraft
