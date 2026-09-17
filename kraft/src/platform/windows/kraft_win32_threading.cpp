#include <platform/kraft_threading.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

#include <core/kraft_core_includes.h>

namespace kraft {

struct Win32ThreadStart {
    ThreadEntry* entry;
    void* user_data;
};

kraft_internal DWORD WINAPI Win32ThreadProc(LPVOID parameter) {
    Win32ThreadStart* start = (Win32ThreadStart*)parameter;
    ThreadEntry* entry = start->entry;
    void* user_data = start->user_data;
    Free(start, sizeof(Win32ThreadStart), MEMORY_TAG_NONE);

    entry(user_data);

    return 0;
}

kraft_internal void Win32SetThreadName(HANDLE thread, String8 name) {
    if (!thread || name.count == 0) {
        return;
    }

    WCHAR wide[64];
    u64 count = name.count < KRAFT_C_ARRAY_SIZE(wide) - 1 ? name.count : KRAFT_C_ARRAY_SIZE(wide) - 1;
    for (u64 index = 0; index < count; index++) {
        wide[index] = (WCHAR)name.ptr[index];
    }
    wide[count] = 0;

    // Read by debuggers - VS and raddbg
    SetThreadDescription(thread, wide);
}

Thread ThreadLaunch(ThreadEntry* entry, void* user_data, String8 name) {
    Thread result = {};

    Win32ThreadStart* start = (Win32ThreadStart*)Malloc(sizeof(Win32ThreadStart), MEMORY_TAG_NONE, false);
    start->entry = entry;
    start->user_data = user_data;

    DWORD thread_id = 0;
    HANDLE handle = CreateThread(nullptr, 0, Win32ThreadProc, start, 0, &thread_id);
    if (!handle) {
        Free(start, sizeof(Win32ThreadStart), MEMORY_TAG_NONE);
        KERROR("[threading] CreateThread failed (win32 err %lu)", GetLastError());
        return result;
    }

    Win32SetThreadName(handle, name);
    result.handle = handle;
    result.id = (u64)thread_id;

    return result;
}

void ThreadJoin(Thread thread) {
    if (!thread.handle) {
        return;
    }

    WaitForSingleObject((HANDLE)thread.handle, INFINITE);
    CloseHandle((HANDLE)thread.handle);
}

void ThreadDetach(Thread thread) {
    if (thread.handle) {
        CloseHandle((HANDLE)thread.handle);
    }
}

u64 ThreadCurrentId() {
    return (u64)GetCurrentThreadId();
}

u32 LogicalCoreCount() {
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);

    return info.dwNumberOfProcessors > 0 ? (u32)info.dwNumberOfProcessors : 1;
}

Semaphore SemaphoreCreate(u32 initial_count, u32 max_count) {
    Semaphore result = {};
    result.handle = CreateSemaphoreA(nullptr, (LONG)initial_count, (LONG)(max_count > 0 ? max_count : 1), nullptr);
    if (!result.handle) {
        KERROR("[threading] CreateSemaphore failed (win32 err %lu)", GetLastError());
    }

    return result;
}

void SemaphoreDestroy(Semaphore semaphore) {
    if (semaphore.handle) {
        CloseHandle((HANDLE)semaphore.handle);
    }
}

b32 SemaphoreTake(Semaphore semaphore, u64 timeout_milliseconds) {
    if (!semaphore.handle) {
        return false;
    }

    DWORD timeout = timeout_milliseconds == KRAFT_WAIT_FOREVER ? INFINITE : (DWORD)timeout_milliseconds;

    return WaitForSingleObject((HANDLE)semaphore.handle, timeout) == WAIT_OBJECT_0;
}

void SemaphoreDrop(Semaphore semaphore, u32 count) {
    if (!semaphore.handle || count == 0) {
        return;
    }

    if (!ReleaseSemaphore((HANDLE)semaphore.handle, (LONG)count, nullptr)) {
        KERROR("[threading] ReleaseSemaphore of %u failed (win32 err %lu)", count, GetLastError());
    }
}

void MutexCreate(Mutex* mutex) {
    static_assert(sizeof(SRWLOCK) == sizeof(void*), "Mutex stores an SRWLOCK inline");
    InitializeSRWLock((PSRWLOCK)&mutex->value);
}

void MutexDestroy(Mutex* mutex) {
    // An SRWLOCK holds no OS resource, so there is nothing to release
    mutex->value = nullptr;
}

void MutexLock(Mutex* mutex) {
    AcquireSRWLockExclusive((PSRWLOCK)&mutex->value);
}

void MutexUnlock(Mutex* mutex) {
    ReleaseSRWLockExclusive((PSRWLOCK)&mutex->value);
}

i64 AtomicIncrement64(volatile i64* target) {
    return (i64)_InterlockedIncrement64((volatile __int64*)target);
}

i64 AtomicDecrement64(volatile i64* target) {
    return (i64)_InterlockedDecrement64((volatile __int64*)target);
}

i64 AtomicAdd64(volatile i64* target, i64 addend) {
    return (i64)_InterlockedExchangeAdd64((volatile __int64*)target, (__int64)addend) + addend;
}

i64 AtomicExchange64(volatile i64* target, i64 value) {
    return (i64)_InterlockedExchange64((volatile __int64*)target, (__int64)value);
}

i64 AtomicCompareExchange64(volatile i64* target, i64 value, i64 comparand) {
    return (i64)_InterlockedCompareExchange64((volatile __int64*)target, (__int64)value, (__int64)comparand);
}

i64 AtomicLoad64(volatile i64* target) {
    return (i64)_InterlockedCompareExchange64((volatile __int64*)target, 0, 0);
}

void AtomicStore64(volatile i64* target, i64 value) {
    _InterlockedExchange64((volatile __int64*)target, (__int64)value);
}

i32 AtomicIncrement32(volatile i32* target) {
    return (i32)_InterlockedIncrement((volatile long*)target);
}

i32 AtomicDecrement32(volatile i32* target) {
    return (i32)_InterlockedDecrement((volatile long*)target);
}

i32 AtomicExchange32(volatile i32* target, i32 value) {
    return (i32)_InterlockedExchange((volatile long*)target, (long)value);
}

i32 AtomicCompareExchange32(volatile i32* target, i32 value, i32 comparand) {
    return (i32)_InterlockedCompareExchange((volatile long*)target, (long)value, (long)comparand);
}

i32 AtomicLoad32(volatile i32* target) {
    return (i32)_InterlockedCompareExchange((volatile long*)target, 0, 0);
}

void AtomicStore32(volatile i32* target, i32 value) {
    _InterlockedExchange((volatile long*)target, (long)value);
}

} // namespace kraft
