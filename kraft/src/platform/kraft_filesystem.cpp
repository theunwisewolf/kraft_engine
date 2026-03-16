#include "kraft_filesystem.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>

#include <core/kraft_base_includes.h>

#if defined(KRAFT_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "Windows.h"
#undef WIN32_LEAN_AND_MEAN
#endif

namespace kraft {

namespace fs {

bool OpenFile(String8 path, int mode, bool binary, FileHandle* result)
{
    KASSERT(result);
    result->Handle = {};

    const char* mode_string;
    if (mode & FILE_OPEN_MODE_APPEND)
    {
        mode_string = binary ? "a+b" : "a+";
    }
    else if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE))
    {
        mode_string = binary ? "w+b" : "w+";
    }
    else if ((mode & FILE_OPEN_MODE_READ) && (mode & FILE_OPEN_MODE_WRITE) == 0)
    {
        mode_string = binary ? "r+b" : "r";
    }
    else if ((mode & FILE_OPEN_MODE_READ) == 0 && (mode & FILE_OPEN_MODE_WRITE))
    {
        mode_string = binary ? "wb" : "w";
    }
    else
    {
        KERROR("[FileSystem::OpenFile] Invalid mode %d", mode);
        return {};
    }

#if UNICODE && defined(KRAFT_PLATFORM_WINDOWS)
    TempArena scratch = ScratchBegin(0, 0);

    int      character_count = MultiByteToWideChar(CP_UTF8, 0, path.str, -1, NULL, 0);
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
    if (!result->Handle)
    {
        KERROR("[FileSystem::OpenFile]: Failed to open %S with error %s", path, strerror(errno));
        return false;
    }

    return true;
}

u64 GetFileSize(FileHandle* handle)
{
    KASSERT(handle->Handle);

    fseek(handle->Handle, 0, SEEK_END);
    u64 size = ftell(handle->Handle);
    rewind(handle->Handle);

    return size;
}

void CloseFile(FileHandle* handle)
{
    if (handle)
    {
        fclose(handle->Handle);
        handle->Handle = 0;
    }
}

bool ReadAllBytes(FileHandle* handle, u8** out_buffer, u64* bytes_read)
{
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
    if (read != size)
    {
        return false;
    }

    if (bytes_read)
    {
        *bytes_read = read;
    }

    return true;
}

buffer ReadAllBytes(ArenaAllocator* arena, FileHandle* handle)
{
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

buffer ReadAllBytes(ArenaAllocator* arena, String8 path)
{
    FileHandle handle = {};
    if (!OpenFile(path, FILE_OPEN_MODE_READ, true, &handle))
    {
        return {};
    }

    buffer result = ReadAllBytes(arena, &handle);
    CloseFile(&handle);

    return result;
}

bool WriteFile(FileHandle* handle, const u8* buffer, u64 size)
{
    if (!handle->Handle)
        return false;

    fwrite(buffer, sizeof(u8), size, handle->Handle);

    return true;
}

String8 CleanPath(ArenaAllocator* arena, String8 path)
{
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, path.count + 1);
    result.count = path.count;

    for (i32 i = 0; i < (i32)path.count; i++)
    {
        if (path.ptr[i] == '\\')
        {
            result.ptr[i] = '/';
        }
        else
        {
            result.ptr[i] = path.ptr[i];
        }
    }

    result.ptr[result.count] = 0;
    return result;
}

String8 Dirname(ArenaAllocator* arena, String8 path)
{
    String8 result = {};
    i32     i = (i32)path.count - 1;
    for (; i >= 0; i--)
    {
        if (path.ptr[i] == '/' || path.ptr[i] == '\\')
        {
            break;
        }
    }

    // This represents the normal case: "xyz/SampleApp.exe"
    if (i > 0)
    {
        result.ptr = ArenaPushArray(arena, u8, i + 1);
        MemCpy(result.ptr, path.ptr, i);
        result.ptr[i] = 0; // Null terminate, because why not
        result.count = i;
    }
    // This is when path is just "/" or "\" (Root)
    else if (i == 0)
    {
        result.ptr = ArenaPushArray(arena, u8, 2);
        result.ptr[0] = path.ptr[0]; // What is at 0, just use that
        result.ptr[1] = 0;
        result.count = 1;
    }
    // If we have a path like "SampleApp.exe" then `i` will be negative
    else
    {
        result.ptr = ArenaPushArray(arena, u8, 2);
        result.ptr[0] = '.';
        result.ptr[1] = 0;
        result.count = 1;
    }

    return result;
}

String8 Basename(ArenaAllocator* arena, String8 path)
{
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, path.count);
    i32 i = (i32)path.count - 1;
    for (; i >= 0; i--)
    {
        if (path.ptr[i] == '/' || path.ptr[i] == '\\')
        {
            break;
        }
    }

    result.count = path.count - i + 1;
    MemCpy(result.ptr, path.ptr + i + 1, result.count);
    ArenaPop(arena, path.count - result.count);

    return result;
}

u64 GetFileModifiedTime(String8 path)
{
#ifdef KRAFT_PLATFORM_WINDOWS
    struct _stat64 file_stat;
    if (_stat64((const char*)path.ptr, &file_stat) != 0)
    {
        return 0;
    }
    return (u64)file_stat.st_mtime;
#else
    struct stat file_stat;
    if (stat((const char*)path.ptr, &file_stat) != 0)
    {
        return 0;
    }
    return (u64)file_stat.st_mtime;
#endif
}

String8 PathJoin(ArenaAllocator* arena, String8 path1, String8 path2)
{
    String8 result = {};
    u64     final_path_size = path1.count + path2.count + sizeof(KRAFT_PATH_SEPARATOR) + 1;
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

String8 AbsolutePath(ArenaAllocator* arena, String8 path)
{
#ifdef KRAFT_PLATFORM_WINDOWS
    char resolved[MAX_PATH];
    if (_fullpath(resolved, (const char*)path.ptr, MAX_PATH) == nullptr)
    {
        KERROR("[FileSystem::AbsolutePath]: Failed to resolve path %S", path);
        return {};
    }
#else
    char resolved[PATH_MAX];
    if (realpath((const char*)path.ptr, resolved) == nullptr)
    {
        KERROR("[FileSystem::AbsolutePath]: Failed to resolve path %S", path);
        return {};
    }
#endif

    u64     len = CStringLength((u8*)resolved);
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, len + 1);
    result.count = len;
    MemCpy(result.ptr, resolved, len);
    result.ptr[len] = 0;

    return result;
}

} // namespace fs

//
// File Watcher
//

namespace fs {

static FileWatcherState* file_watcher_state = nullptr;

bool FileWatcher::Init(ArenaAllocator* arena)
{
    file_watcher_state = ArenaPush(arena, FileWatcherState);
    file_watcher_state->arena = arena;
    file_watcher_state->watch_count = 0;

    return true;
}

void FileWatcher::Shutdown()
{
    for (u32 i = 0; i < file_watcher_state->watch_count; i++)
    {
        UnwatchDirectory(i);
    }

    file_watcher_state = nullptr;
}

#if defined(KRAFT_PLATFORM_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

struct Win32WatchData
{
    HANDLE     directory_handle;
    OVERLAPPED overlapped;
    u8         buffer[4096];
    bool       active;
};

i32 FileWatcher::WatchDirectory(String8 directory, bool recursive, FileWatchCallback callback, void* userdata)
{
    if (file_watcher_state->watch_count >= FILE_WATCHER_MAX_WATCHES)
    {
        KERROR("[FileWatcher]: Max watches reached (%d)", FILE_WATCHER_MAX_WATCHES);
        return -1;
    }

    i32             index = file_watcher_state->watch_count;
    FileWatchEntry* entry = &file_watcher_state->watches[index];
    Win32WatchData* watch_data = ArenaPush(file_watcher_state->arena, Win32WatchData);

    HANDLE dir_handle =
        CreateFileA(directory.str, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

    if (dir_handle == INVALID_HANDLE_VALUE)
    {
        KERROR("[FileWatcher]: Failed to open directory '%S' (error: %d)", directory, GetLastError());
        return -1;
    }

    watch_data->directory_handle = dir_handle;
    watch_data->active = true;
    MemZero(&watch_data->overlapped, sizeof(OVERLAPPED));

    BOOL success = ReadDirectoryChangesW(
        dir_handle,
        watch_data->buffer,
        sizeof(watch_data->buffer),
        recursive ? TRUE : FALSE,
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
        NULL,
        &watch_data->overlapped,
        NULL
    );

    if (!success)
    {
        KERROR("[FileWatcher]: ReadDirectoryChangesW failed (error: %d)", GetLastError());
        CloseHandle(dir_handle);
        return -1;
    }

    entry->directory = directory;
    entry->callback = callback;
    entry->userdata = userdata;
    entry->recursive = recursive;
    entry->platform_data = watch_data;

    file_watcher_state->watch_count++;

    KINFO("[FileWatcher]: Watching '%S'", directory);
    return index;
}

void FileWatcher::UnwatchDirectory(i32 watch_index)
{
    if (watch_index < 0 || watch_index >= (i32)file_watcher_state->watch_count)
        return;

    FileWatchEntry* entry = &file_watcher_state->watches[watch_index];
    Win32WatchData* watch_data = (Win32WatchData*)entry->platform_data;

    if (watch_data && watch_data->active)
    {
        CancelIo(watch_data->directory_handle);
        CloseHandle(watch_data->directory_handle);
        watch_data->active = false;
    }
}

void FileWatcher::ProcessChanges()
{
    if (!file_watcher_state)
        return;

    TempArena scratch = ScratchBegin(&file_watcher_state->arena, 1);

    for (u32 i = 0; i < file_watcher_state->watch_count; i++)
    {
        FileWatchEntry* entry = &file_watcher_state->watches[i];
        Win32WatchData* watch_data = (Win32WatchData*)entry->platform_data;
        if (!watch_data || !watch_data->active)
            continue;

        DWORD bytes_transferred = 0;
        BOOL  result = GetOverlappedResult(watch_data->directory_handle, &watch_data->overlapped, &bytes_transferred, FALSE);

        if (!result || bytes_transferred == 0)
            continue;

        // Process the notification buffer
        FILE_NOTIFY_INFORMATION* notify = (FILE_NOTIFY_INFORMATION*)watch_data->buffer;
        for (;;)
        {
            // Convert wide filename to UTF-8
            int   utf8_len = WideCharToMultiByte(CP_UTF8, 0, notify->FileName, notify->FileNameLength / sizeof(WCHAR), NULL, 0, NULL, NULL);
            char* utf8_name = ArenaPushArray(scratch.arena, char, utf8_len + 1);
            WideCharToMultiByte(CP_UTF8, 0, notify->FileName, notify->FileNameLength / sizeof(WCHAR), utf8_name, utf8_len, NULL, NULL);
            utf8_name[utf8_len] = 0;

            String8 relative_path = String8FromPtrAndLength((u8*)utf8_name, utf8_len);
            String8 full_path = PathJoin(scratch.arena, entry->directory, relative_path);

            FileWatchEventType event_type;
            switch (notify->Action)
            {
                case FILE_ACTION_MODIFIED:         event_type = FILE_WATCH_EVENT_MODIFIED; break;
                case FILE_ACTION_ADDED:            event_type = FILE_WATCH_EVENT_CREATED; break;
                case FILE_ACTION_REMOVED:          event_type = FILE_WATCH_EVENT_DELETED; break;
                case FILE_ACTION_RENAMED_OLD_NAME:
                case FILE_ACTION_RENAMED_NEW_NAME: event_type = FILE_WATCH_EVENT_RENAMED; break;
                default:                           event_type = FILE_WATCH_EVENT_MODIFIED; break;
            }

            FileWatchEvent event = {};
            event.type = event_type;
            event.file_path = full_path;
            event.directory = entry->directory;

            entry->callback(event, entry->userdata);

            if (notify->NextEntryOffset == 0)
                break;

            notify = (FILE_NOTIFY_INFORMATION*)((u8*)notify + notify->NextEntryOffset);
        }

        // Re-arm the watch
        MemZero(&watch_data->overlapped, sizeof(OVERLAPPED));
        ReadDirectoryChangesW(
            watch_data->directory_handle,
            watch_data->buffer,
            sizeof(watch_data->buffer),
            entry->recursive ? TRUE : FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
            NULL,
            &watch_data->overlapped,
            NULL
        );
    }

    ScratchEnd(scratch);
}

#else

// TODO (amn):
// Stub implementations for other platforms
i32 FileWatcher::WatchDirectory(String8 directory, bool recursive, FileWatchCallback callback, void* userdata)
{
    return -1;
}

void FileWatcher::UnwatchDirectory(i32 watch_index)
{}

void FileWatcher::ProcessChanges()
{}

#endif

} // namespace fs

} // namespace kraft