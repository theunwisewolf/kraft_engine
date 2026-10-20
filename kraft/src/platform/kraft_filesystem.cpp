#include "kraft_filesystem.h"
#include "kraft_filesystem_internal.h"
#include "kraft_platform.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>

#include <core/kraft_core_includes.h>

#if defined(KRAFT_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "Windows.h"
#undef WIN32_LEAN_AND_MEAN
#endif

namespace kraft {

namespace fs {

bool OpenFile(String8 path, int mode, bool binary, FileHandle* result) {
    KASSERT(result);
    result->Handle = {};

    const char* mode_string;
    if (mode & FILE_OPEN_MODE_APPEND) {
        mode_string = binary ? "a+b" : "a+";
    } else if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE)) {
        mode_string = binary ? "w+b" : "w+";
    } else if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE) == 0) {
        mode_string = binary ? "rb" : "r";
    } else if ((mode & FILE_OPEN_MODE_READ) == 0 && (mode & FILE_OPEN_MODE_WRITE)) {
        mode_string = binary ? "wb" : "w";
    } else {
        KERROR("[FileSystem::OpenFile] Invalid mode %d", mode);
        return {};
    }

#if UNICODE && defined(KRAFT_PLATFORM_WINDOWS)
    TempArena scratch = ScratchBegin(0, 0);

    int character_count = MultiByteToWideChar(CP_UTF8, 0, path.str, -1, NULL, 0);
    wchar_t* path_wide = ArenaPushArray(scratch.arena, wchar_t, character_count);
    KASSERT(MultiByteToWideChar(CP_UTF8, 0, path.str, -1, path_wide, character_count));

    character_count = MultiByteToWideChar(CP_UTF8, 0, mode_string, -1, NULL, 0);
    wchar_t* mode_wide = ArenaPushArray(scratch.arena, wchar_t, character_count);
    KASSERT(MultiByteToWideChar(CP_UTF8, 0, mode_string, -1, mode_wide, character_count));

    result->Handle = _wfopen(path_wide, mode_wide);

    ScratchEnd(scratch);
#else
    result->Handle = fopen((char*)path.ptr, mode_string);
#endif

#if !defined(KRAFT_PLATFORM_WINDOWS)
    // A folder opens like a file here, as it does not on Windows
    struct stat file_stat;
    if (result->Handle && fstat(fileno(result->Handle), &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
        fclose(result->Handle);
        result->Handle = nullptr;
        errno = EISDIR;
    }
#endif

    if (!result->Handle) {
        KERROR("[FileSystem::OpenFile]: Failed to open %S with error %s", path, strerror(errno));
        return false;
    }

    return true;
}

u64 GetFileSize(FileHandle* handle) {
    KASSERT(handle->Handle);

    fseek(handle->Handle, 0, SEEK_END);
    u64 size = ftell(handle->Handle);
    rewind(handle->Handle);

    return size;
}

void CloseFile(FileHandle* handle) {
    if (handle) {
        fclose(handle->Handle);
        handle->Handle = 0;
    }
}

bool ReadAllBytes(FileHandle* handle, u8** out_buffer, u64* bytes_read) {
    if (!handle->Handle)
        return false;

    fseek(handle->Handle, 0, SEEK_END);
    u64 size = ftell(handle->Handle);
    rewind(handle->Handle);

    if (!*out_buffer)
        *out_buffer = (u8*)Malloc(size, MEMORY_TAG_FILE_BUF);

    size_t read = fread(*out_buffer, 1, size, handle->Handle);

    KASSERT(read == size);

    // Not all bytes were read
    if (read != size) {
        return false;
    }

    if (bytes_read) {
        *bytes_read = read;
    }

    return true;
}

buffer ReadAllBytes(ArenaAllocator* arena, FileHandle* handle) {
    fseek(handle->Handle, 0, SEEK_END);
    u64 size = ftell(handle->Handle);
    rewind(handle->Handle);

    buffer output = {};
    output.ptr = ArenaPushArray(arena, u8, size);
    output.count = size;

    size_t read = fread(output.ptr, 1, size, handle->Handle);
    KASSERT(read == size);

    return output;
}

buffer ReadAllBytes(ArenaAllocator* arena, String8 path) {
    FileHandle handle = {};
    if (!OpenFile(path, FILE_OPEN_MODE_READ, true, &handle)) {
        return {};
    }

    buffer result = ReadAllBytes(arena, &handle);
    CloseFile(&handle);

    return result;
}

bool WriteFile(FileHandle* handle, const u8* buffer, u64 size) {
    if (!handle->Handle)
        return false;

    fwrite(buffer, sizeof(u8), size, handle->Handle);

    return true;
}

// https://github.com/golang/go/blob/master/src/internal/filepathlite/path.go
String8 CleanPath(ArenaAllocator* arena, String8 path) {
    const u8 sep = KRAFT_PATH_SEPARATOR;

    if (path.count == 0) {
        String8 dot = {};
        dot.ptr = ArenaPushArray(arena, u8, 2);
        dot.ptr[0] = '.';
        dot.ptr[1] = 0;
        dot.count = 1;

        return dot;
    }

    u8* buf = ArenaPushArray(arena, u8, path.count + 1);
    u32 write_index = 0;

    bool rooted = (path.ptr[0] == '/' || path.ptr[0] == '\\');

    // Does the path include a drive letter prefix?
    bool has_drive = false;
    if (path.count >= 2 && ((path.ptr[0] >= 'A' && path.ptr[0] <= 'Z') || (path.ptr[0] >= 'a' && path.ptr[0] <= 'z')) && path.ptr[1] == ':') {
        has_drive = true;
        buf[write_index++] = path.ptr[0];
        buf[write_index++] = ':';
        if (path.count >= 3 && (path.ptr[2] == '/' || path.ptr[2] == '\\')) {
            rooted = true;
        }
    }

    if (rooted) {
        buf[write_index++] = sep;
    }

    u32 dot_dot_limit = write_index; // We cannot backtrack past this point

    u32 read_index = has_drive ? (rooted ? 3 : 2) : (rooted ? 1 : 0);

    while (read_index < path.count) {
        u8 c = path.ptr[read_index];

        if (c == '/' || c == '\\') {
            // Skip redundant slashes
            read_index++;
        } else if (c == '.' && (read_index + 1 == path.count || path.ptr[read_index + 1] == '/' || path.ptr[read_index + 1] == '\\')) {
            // Single dot element "."
            read_index++;
        } else if (c == '.' && path.ptr[read_index + 1] == '.' && (read_index + 2 == path.count || path.ptr[read_index + 2] == '/' || path.ptr[read_index + 2] == '\\')) {
            // Double dot element ".."
            read_index += 2;

            // Try to backtrack
            if (write_index > dot_dot_limit) {
                // Back up to previous slash
                write_index--;
                while (write_index > dot_dot_limit && buf[write_index - 1] != sep) {
                    write_index--;
                }
            } else if (!rooted) {
                // Cannot backtrack, emit ".."
                if (write_index > 0 && buf[write_index - 1] != sep) {
                    buf[write_index++] = sep;
                }

                buf[write_index++] = '.';
                buf[write_index++] = '.';
                dot_dot_limit = write_index;
            }
        } else {
            // Normal path element
            if ((rooted && write_index != dot_dot_limit) || (!rooted && write_index > 0)) {
                buf[write_index++] = sep;
            }

            while (read_index < path.count && path.ptr[read_index] != '/' && path.ptr[read_index] != '\\') {
                buf[write_index++] = path.ptr[read_index++];
            }
        }
    }

    if (write_index == 0) {
        buf[write_index++] = '.';
    }

    buf[write_index] = 0;

    // Return unused memory
    ArenaPop(arena, path.count + 1 - (write_index + 1));

    String8 result = {};
    result.ptr = buf;
    result.count = write_index;
    return result;
}

String8 Dirname(ArenaAllocator* arena, String8 path) {
    String8 result = {};
    i32 i = (i32)path.count - 1;
    for (; i >= 0; i--) {
        if (path.ptr[i] == '/' || path.ptr[i] == '\\') {
            break;
        }
    }

    // This represents the normal case: "xyz/SampleApp.exe"
    if (i > 0) {
        result.ptr = ArenaPushArray(arena, u8, i + 1);
        MemCpy(result.ptr, path.ptr, i);
        result.ptr[i] = 0; // Null terminate, because why not
        result.count = i;
    }
    // This is when path is just "/" or "\" (Root)
    else if (i == 0) {
        result.ptr = ArenaPushArray(arena, u8, 2);
        result.ptr[0] = path.ptr[0]; // What is at 0, just use that
        result.ptr[1] = 0;
        result.count = 1;
    }
    // If we have a path like "SampleApp.exe" then `i` will be negative
    else {
        result.ptr = ArenaPushArray(arena, u8, 2);
        result.ptr[0] = '.';
        result.ptr[1] = 0;
        result.count = 1;
    }

    return result;
}

String8 Basename(ArenaAllocator* arena, String8 path) {
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, path.count);
    i32 i = (i32)path.count - 1;
    for (; i >= 0; i--) {
        if (path.ptr[i] == '/' || path.ptr[i] == '\\') {
            break;
        }
    }

    // i is the index of the separator, or -1 when there is none, so the name starts at i + 1
    result.count = path.count - (u64)(i + 1);
    MemCpy(result.ptr, path.ptr + i + 1, result.count);
    ArenaPop(arena, path.count - result.count);

    return result;
}

u64 GetFileModifiedTime(String8 path) {
#ifdef KRAFT_PLATFORM_WINDOWS
    struct _stat64 file_stat;
    if (_stat64((const char*)path.ptr, &file_stat) != 0) {
        return 0;
    }
    return (u64)file_stat.st_mtime;
#else
    struct stat file_stat;
    if (stat((const char*)path.ptr, &file_stat) != 0) {
        return 0;
    }
    return (u64)file_stat.st_mtime;
#endif
}

String8 PathJoin(ArenaAllocator* arena, String8 path1, String8 path2) {
    String8 result = {};
    u64 final_path_size = path1.count + path2.count + sizeof(KRAFT_PATH_SEPARATOR) + 1;
    result.ptr = ArenaPushArray(arena, u8, final_path_size);
    u8* ptr = result.ptr;

    MemCpy(ptr, path1.ptr, path1.count);
    ptr += path1.count;

    // TODO (amn): Dont add separators if path1 ends with a separator
    *ptr = KRAFT_PATH_SEPARATOR;
    ptr += 1;

    MemCpy(ptr, path2.ptr, path2.count);
    ptr += path2.count;

    *ptr = 0;

    KASSERT((ptr - result.ptr + 1) == final_path_size);
    result.count = final_path_size - 1;

    return result;
}

String8 AbsolutePath(ArenaAllocator* arena, String8 path) {
#ifdef KRAFT_PLATFORM_WINDOWS
    char resolved[MAX_PATH];
    if (_fullpath(resolved, (const char*)path.ptr, MAX_PATH) == nullptr) {
        KERROR("[FileSystem::AbsolutePath]: Failed to resolve path %S", path);
        return {};
    }
#else
    char resolved[PATH_MAX];
    if (realpath((const char*)path.ptr, resolved) == nullptr) {
        KERROR("[FileSystem::AbsolutePath]: Failed to resolve path %S", path);
        return {};
    }
#endif

    u64 len = CStringLength((u8*)resolved);
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, len + 1);
    result.count = len;
    MemCpy(result.ptr, resolved, len);
    result.ptr[len] = 0;

    return result;
}

bool RemoveTree(String8 path) {
    if (IsLink(path)) {
        return RemoveLink(path);
    }

    if (!DirectoryExists(path)) {
        return RemoveFile(path);
    }

    TempArena scratch = ScratchBegin(0, 0);
    Directory listing = ReadDirEntries(scratch.arena, path);

    bool removed = true;
    for (u32 index = 0; index < listing.entry_count; index++) {
        FileSystemEntry* entry = &listing.entries[index];
        String8 child = StringFormat(scratch.arena, "%S/%S", path, entry->name);
        removed = (entry->is_link ? RemoveLink(child) : entry->is_directory ? RemoveTree(child) : RemoveFile(child)) && removed;
    }

    removed = removed && RemoveEmptyDirectory(path);
    ScratchEnd(scratch);

    return removed;
}

} // namespace fs

//
// Directory watch
//

namespace fs {

#define KRAFT_DIRECTORY_WATCH_QUEUE_BYTES KRAFT_SIZE_MB(1)

DirectoryWatch* DirectoryWatchCreate(bool recursive, DirectoryWatchIgnore* ignore, DirectoryWatchWake* wake) {
    ArenaAllocator* arena = ArenaAlloc();
    DirectoryWatch* watch = ArenaPush(arena, DirectoryWatch);
    watch->arena = arena;
    watch->recursive = recursive;
    watch->ignore = ignore;
    watch->wake = wake;
    watch->queue = ArenaPushArrayNoZero(arena, u8, KRAFT_DIRECTORY_WATCH_QUEUE_BYTES);
    MutexCreate(&watch->lock);

    return watch;
}

void DirectoryWatchDestroy(DirectoryWatch* watch) {
    MutexDestroy(&watch->lock);
    DestroyArena(watch->arena);
}

bool DirectoryWatchHasPending(DirectoryWatch* watch) {
    return watch->queue_used > 0 || watch->overflowed || (watch->lost && !watch->lost_told);
}

void DirectoryWatchStamp(DirectoryWatch* watch, bool was_pending) {
    f64 now = Platform::GetAbsoluteTime();
    if (!was_pending) {
        watch->first_time = now;
    }

    watch->last_time = now;
}

void DirectoryWatchPush(DirectoryWatch* watch, String8 relative_path) {
    if (watch->queue_used + sizeof(u32) + relative_path.count > KRAFT_DIRECTORY_WATCH_QUEUE_BYTES) {
        watch->overflowed = true;
        return;
    }

    u32 length = (u32)relative_path.count;
    MemCpy(watch->queue + watch->queue_used, &length, sizeof(u32));
    MemCpy(watch->queue + watch->queue_used + sizeof(u32), relative_path.ptr, length);
    watch->queue_used += sizeof(u32) + length;
}

bool DirectoryWatchHasChanges(DirectoryWatch* watch, f64* out_seconds_since_last, f64* out_seconds_since_first) {
    if (!watch) {
        return false;
    }

    MutexLock(&watch->lock);
    bool pending = DirectoryWatchHasPending(watch);
    f64 first_time = watch->first_time;
    f64 last_time = watch->last_time;
    MutexUnlock(&watch->lock);
    if (pending) {
        f64 now = Platform::GetAbsoluteTime();
        *out_seconds_since_last = now - last_time;
        *out_seconds_since_first = now - first_time;
    }

    return pending;
}

DirectoryWatchChanges DirectoryWatchTakeChanges(DirectoryWatch* watch, ArenaAllocator* arena) {
    DirectoryWatchChanges changes = {};
    if (!watch) {
        return changes;
    }

    MutexLock(&watch->lock);
    u32 count = 0;
    for (u64 at = 0; at < watch->queue_used;) {
        u32 length = 0;
        MemCpy(&length, watch->queue + at, sizeof(u32));
        at += sizeof(u32) + length;
        count++;
    }

    changes.paths = count > 0 ? ArenaPushArray(arena, String8, count) : nullptr;
    for (u64 at = 0; at < watch->queue_used;) {
        u32 length = 0;
        MemCpy(&length, watch->queue + at, sizeof(u32));
        String8 path = ArenaPushString8Empty(arena, length);
        MemCpy(path.ptr, watch->queue + at + sizeof(u32), length);
        changes.paths[changes.path_count++] = path;
        at += sizeof(u32) + length;
    }

    changes.overflowed = watch->overflowed;
    changes.lost = watch->lost && !watch->lost_told;
    watch->lost_told = watch->lost;
    watch->queue_used = 0;
    watch->overflowed = false;
    MutexUnlock(&watch->lock);

    return changes;
}

void DirectoryWatchPutBack(DirectoryWatch* watch, String8* paths, u32 path_count) {
    if (!watch || path_count == 0) {
        return;
    }

    MutexLock(&watch->lock);
    bool was_pending = DirectoryWatchHasPending(watch);
    for (u32 index = 0; index < path_count; index++) {
        DirectoryWatchPush(watch, paths[index]);
    }

    DirectoryWatchStamp(watch, was_pending);
    MutexUnlock(&watch->lock);
}

} // namespace fs

} // namespace kraft
