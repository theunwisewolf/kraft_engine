#include <platform/kraft_threading.h>

#include <dispatch/dispatch.h>
#include <os/lock.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <core/kraft_core_includes.h>

namespace kraft {

struct MacThreadStart {
    ThreadEntry* entry;
    void* user_data;
    char name[64];
};

kraft_internal void* MacThreadProc(void* parameter) {
    MacThreadStart* start = (MacThreadStart*)parameter;
    ThreadEntry* entry = start->entry;
    void* user_data = start->user_data;

    // Read by debuggers and by Instruments; a thread can only name itself here
    if (start->name[0]) {
        pthread_setname_np(start->name);
    }

    Free(start, sizeof(MacThreadStart), MEMORY_TAG_NONE);

    // A stack of the thread's own for the crash handler, which a thread overflowing its stack has none of to run on
    stack_t alternate = {};
    alternate.ss_size = SIGSTKSZ;
    alternate.ss_sp = malloc(alternate.ss_size);
    if (alternate.ss_sp) {
        sigaltstack(&alternate, nullptr);
    }

    entry(user_data);

    if (alternate.ss_sp) {
        stack_t disabled = {};
        disabled.ss_flags = SS_DISABLE;
        sigaltstack(&disabled, nullptr);
        free(alternate.ss_sp);
    }

    return nullptr;
}

Thread ThreadLaunch(ThreadEntry* entry, void* user_data, String8 name) {
    Thread result = {};

    MacThreadStart* start = (MacThreadStart*)Malloc(sizeof(MacThreadStart), MEMORY_TAG_NONE, false);
    start->entry = entry;
    start->user_data = user_data;
    u64 name_count = name.count < sizeof(start->name) - 1 ? name.count : sizeof(start->name) - 1;
    MemCpy(start->name, name.ptr, name_count);
    start->name[name_count] = 0;

    // The stack a thread gets on Windows; a Mac otherwise gives a thread half of it
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, KRAFT_SIZE_MB(1));
    pthread_t thread = nullptr;
    int error = pthread_create(&thread, &attributes, MacThreadProc, start);
    pthread_attr_destroy(&attributes);
    if (error != 0) {
        Free(start, sizeof(MacThreadStart), MEMORY_TAG_NONE);
        KERROR("[threading] pthread_create failed (errno %d)", error);
        return result;
    }

    u64 thread_id = 0;
    pthread_threadid_np(thread, &thread_id);
    result.handle = (void*)thread;
    result.id = thread_id;

    return result;
}

void ThreadJoin(Thread thread) {
    if (!thread.handle) {
        return;
    }

    pthread_join((pthread_t)thread.handle, nullptr);
}

void ThreadDetach(Thread thread) {
    if (thread.handle) {
        pthread_detach((pthread_t)thread.handle);
    }
}

u64 ThreadCurrentId() {
    u64 thread_id = 0;
    pthread_threadid_np(nullptr, &thread_id);

    return thread_id;
}

u32 LogicalCoreCount() {
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    return count > 0 ? (u32)count : 1;
}

// A dispatch semaphore cannot be destroyed while its count is below the one it was made with, so it is made
// empty and then given its initial count
Semaphore SemaphoreCreate(u32 initial_count, u32 max_count) {
    (void)max_count;
    Semaphore result = {};
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    for (u32 index = 0; index < initial_count; index++) {
        dispatch_semaphore_signal(semaphore);
    }

    result.handle = (void*)semaphore;

    return result;
}

void SemaphoreDestroy(Semaphore semaphore) {
    if (semaphore.handle) {
        dispatch_release((dispatch_semaphore_t)semaphore.handle);
    }
}

b32 SemaphoreTake(Semaphore semaphore, u64 timeout_milliseconds) {
    if (!semaphore.handle) {
        return false;
    }

    dispatch_time_t timeout = timeout_milliseconds == KRAFT_WAIT_FOREVER ? DISPATCH_TIME_FOREVER : dispatch_time(DISPATCH_TIME_NOW, (i64)timeout_milliseconds * (i64)NSEC_PER_MSEC);

    return dispatch_semaphore_wait((dispatch_semaphore_t)semaphore.handle, timeout) == 0;
}

void SemaphoreDrop(Semaphore semaphore, u32 count) {
    if (!semaphore.handle) {
        return;
    }

    for (u32 index = 0; index < count; index++) {
        dispatch_semaphore_signal((dispatch_semaphore_t)semaphore.handle);
    }
}

// An os_unfair_lock is four bytes and unlocked when zeroed, so like Windows' SRW lock it lives inside the Mutex
void MutexCreate(Mutex* mutex) {
    static_assert(sizeof(os_unfair_lock) <= sizeof(mutex->value), "Mutex stores an os_unfair_lock inline");
    mutex->value = nullptr;
    *(os_unfair_lock*)&mutex->value = OS_UNFAIR_LOCK_INIT;
}

void MutexDestroy(Mutex* mutex) {
    mutex->value = nullptr;
}

void MutexLock(Mutex* mutex) {
    os_unfair_lock_lock((os_unfair_lock*)&mutex->value);
}

void MutexUnlock(Mutex* mutex) {
    os_unfair_lock_unlock((os_unfair_lock*)&mutex->value);
}

i64 AtomicIncrement64(volatile i64* target) {
    return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST);
}

i64 AtomicDecrement64(volatile i64* target) {
    return __atomic_sub_fetch(target, 1, __ATOMIC_SEQ_CST);
}

i64 AtomicAdd64(volatile i64* target, i64 addend) {
    return __atomic_add_fetch(target, addend, __ATOMIC_SEQ_CST);
}

i64 AtomicExchange64(volatile i64* target, i64 value) {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

i64 AtomicCompareExchange64(volatile i64* target, i64 value, i64 comparand) {
    __atomic_compare_exchange_n(target, &comparand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

    return comparand;
}

i64 AtomicLoad64(volatile i64* target) {
    return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

void AtomicStore64(volatile i64* target, i64 value) {
    __atomic_store_n(target, value, __ATOMIC_SEQ_CST);
}

void* AtomicExchangePointer(void* volatile* target, void* value) {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

i32 AtomicIncrement32(volatile i32* target) {
    return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST);
}

i32 AtomicDecrement32(volatile i32* target) {
    return __atomic_sub_fetch(target, 1, __ATOMIC_SEQ_CST);
}

i32 AtomicExchange32(volatile i32* target, i32 value) {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

i32 AtomicCompareExchange32(volatile i32* target, i32 value, i32 comparand) {
    __atomic_compare_exchange_n(target, &comparand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

    return comparand;
}

i32 AtomicLoad32(volatile i32* target) {
    return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

void AtomicStore32(volatile i32* target, i32 value) {
    __atomic_store_n(target, value, __ATOMIC_SEQ_CST);
}

} // namespace kraft
