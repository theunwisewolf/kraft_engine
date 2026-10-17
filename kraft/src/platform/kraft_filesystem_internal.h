#pragma once

#include "kraft_filesystem.h"
#include "kraft_threading.h"

namespace kraft::fs {

struct DirectoryWatch {
    ArenaAllocator* arena;
    DirectoryWatchIgnore* ignore;
    DirectoryWatchWake* wake;
    bool recursive;
    void* platform;

    Mutex lock;
    u8* queue; // a u32 length, then that many bytes, per path
    u64 queue_used;
    bool overflowed;
    bool lost;
    bool lost_told;
    f64 first_time;
    f64 last_time;
};

DirectoryWatch* DirectoryWatchCreate(bool recursive, DirectoryWatchIgnore* ignore, DirectoryWatchWake* wake);
void DirectoryWatchDestroy(DirectoryWatch* watch);
bool DirectoryWatchHasPending(DirectoryWatch* watch);
void DirectoryWatchStamp(DirectoryWatch* watch, bool was_pending);
void DirectoryWatchPush(DirectoryWatch* watch, String8 relative_path);

} // namespace kraft::fs
