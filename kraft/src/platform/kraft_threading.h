#pragma once

#include <core/kraft_core.h>

namespace kraft {

#define KRAFT_WAIT_FOREVER 0xFFFFFFFFFFFFFFFFull

// Every handle below is pointer sized on win32
struct Thread {
    void* handle;
    u64 id;
};

struct Semaphore {
    void* handle;
};

struct Mutex {
    void* value;
};

#define KRAFT_THREAD_ENTRY(name) void name(void* user_data)
typedef KRAFT_THREAD_ENTRY(ThreadEntry);

KRAFT_API Thread ThreadLaunch(ThreadEntry* entry, void* user_data, String8 name);
KRAFT_API void ThreadJoin(Thread thread);
KRAFT_API void ThreadDetach(Thread thread);
KRAFT_API u64 ThreadCurrentId();
KRAFT_API u32 LogicalCoreCount();

KRAFT_API Semaphore SemaphoreCreate(u32 initial_count, u32 max_count);
KRAFT_API void SemaphoreDestroy(Semaphore semaphore);
KRAFT_API b32 SemaphoreTake(Semaphore semaphore, u64 timeout_milliseconds);
KRAFT_API void SemaphoreDrop(Semaphore semaphore, u32 count);

KRAFT_API void MutexCreate(Mutex* mutex);
KRAFT_API void MutexDestroy(Mutex* mutex);
KRAFT_API void MutexLock(Mutex* mutex);
KRAFT_API void MutexUnlock(Mutex* mutex);

KRAFT_API i64 AtomicIncrement64(volatile i64* target);
KRAFT_API i64 AtomicDecrement64(volatile i64* target);
KRAFT_API i64 AtomicAdd64(volatile i64* target, i64 addend);
KRAFT_API i64 AtomicExchange64(volatile i64* target, i64 value);
KRAFT_API i64 AtomicCompareExchange64(volatile i64* target, i64 value, i64 comparand);
KRAFT_API i64 AtomicLoad64(volatile i64* target);
KRAFT_API void AtomicStore64(volatile i64* target, i64 value);

KRAFT_API i32 AtomicIncrement32(volatile i32* target);
KRAFT_API i32 AtomicDecrement32(volatile i32* target);
KRAFT_API i32 AtomicExchange32(volatile i32* target, i32 value);
KRAFT_API i32 AtomicCompareExchange32(volatile i32* target, i32 value, i32 comparand);
KRAFT_API i32 AtomicLoad32(volatile i32* target);
KRAFT_API void AtomicStore32(volatile i32* target, i32 value);

} // namespace kraft
