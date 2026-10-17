#include <platform/kraft_filesystem.h>
#include <platform/kraft_filesystem_internal.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>

#include <core/kraft_core_includes.h>

#pragma comment(lib, "shell32")

namespace kraft::fs {

static WCHAR* Win32WidePath(ArenaAllocator* arena, String8 path) {
    i32 wide_count = MultiByteToWideChar(CP_UTF8, 0, (const char*)path.ptr, (i32)path.count, nullptr, 0);

    // Double zero-terminated, as SHFileOperation wants
    WCHAR* wide = ArenaPushArray(arena, WCHAR, (u64)wide_count + 2);
    MultiByteToWideChar(CP_UTF8, 0, (const char*)path.ptr, (i32)path.count, wide, wide_count);

    return wide;
}

// FileMMapHandle MMap(const String& Path) {
//     HANDLE File = CreateFileA(*Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

//     if (File == INVALID_HANDLE_VALUE) {
//         return {0};
//     }

//     HANDLE FileMapping = CreateFileMapping(File, NULL, PAGE_READONLY, 0, 0, NULL);
//     KASSERT(FileMapping != INVALID_HANDLE_VALUE);

//     LPVOID FileMapView = MapViewOfFile(FileMapping, FILE_MAP_READ, 0, 0, 0);
//     KASSERT(FileMapView != nullptr);

//     FileMMapHandle Out;
//     Out.Handle = FileMapView;
//     Out.Size = (u64)(::GetFileSize(File, NULL));

//     return Out;
// }

KRAFT_API bool FileExists(String8 path) {
    DWORD attributes = GetFileAttributes((char*)path.ptr);

    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

KRAFT_API bool DirectoryExists(String8 path) {
    DWORD attributes = GetFileAttributes((char*)path.ptr);

    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

KRAFT_API bool MakeDirectory(String8 path) {
    if (CreateDirectoryA((char*)path.ptr, nullptr)) {
        return true;
    }

    return GetLastError() == ERROR_ALREADY_EXISTS;
}

KRAFT_API bool MakeDirectories(ArenaAllocator* arena, String8 path) {
    TempArena scratch = ScratchBegin(&arena, 1);
    String8 cleaned = CleanPath(scratch.arena, path);
    bool ok = true;

    for (u64 index = 1; index < cleaned.count && ok; index++) {
        if (cleaned.str[index] != '/' && cleaned.str[index] != '\\') {
            continue;
        }

        if (cleaned.str[index - 1] == ':') {
            continue;
        }

        String8 prefix = ArenaPushString8Copy(scratch.arena, String8FromPtrAndLength(cleaned.ptr, index));
        ok = MakeDirectory(prefix);
    }

    if (ok) {
        ok = MakeDirectory(cleaned);
    }
    ScratchEnd(scratch);

    return ok;
}

KRAFT_API bool RemoveFile(String8 path) {
    if (DeleteFileA((char*)path.ptr)) {
        return true;
    }

    DWORD error = GetLastError();
    // Read-only files too, as on other systems
    if (error == ERROR_ACCESS_DENIED && SetFileAttributesA((char*)path.ptr, FILE_ATTRIBUTE_NORMAL)) {
        if (DeleteFileA((char*)path.ptr)) {
            return true;
        }

        error = GetLastError();
    }

    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

KRAFT_API bool MovePath(String8 from, String8 to, bool replace_existing) {
    TempArena scratch = ScratchBegin(0, 0);
    bool moved = MoveFileExW(Win32WidePath(scratch.arena, from), Win32WidePath(scratch.arena, to), replace_existing ? MOVEFILE_REPLACE_EXISTING : 0) != 0;
    ScratchEnd(scratch);

    return moved;
}

KRAFT_API bool MoveToTrash(String8 path) {
    TempArena scratch = ScratchBegin(0, 0);
    String8 native = ArenaPushString8Copy(scratch.arena, path);
    for (u64 index = 0; index < native.count; index++) {
        if (native.ptr[index] == '/') {
            native.ptr[index] = '\\';
        }
    }

    SHFILEOPSTRUCTW operation = {};
    operation.wFunc = FO_DELETE;
    operation.pFrom = Win32WidePath(scratch.arena, native);
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    bool trashed = SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted;
    ScratchEnd(scratch);

    return trashed;
}

KRAFT_API FileReadStatus ReadEntireFile(ArenaAllocator* arena, String8 path, String8* out_contents) {
    *out_contents = String8{};
    TempArena scratch = ScratchBegin(&arena, 1);
    HANDLE file = CreateFileW(Win32WidePath(scratch.arena, path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    ScratchEnd(scratch);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? FILE_READ_STATUS_MISSING : FILE_READ_STATUS_BUSY;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 0x7FFFFFFF) {
        CloseHandle(file);
        return FILE_READ_STATUS_BUSY;
    }

    u64 capacity = (u64)size.QuadPart;
    u8* bytes = ArenaPushArrayNoZero(arena, u8, capacity + 1);
    u64 total = 0;
    while (total < capacity) {
        DWORD got = 0;
        if (!::ReadFile(file, bytes + total, (DWORD)(capacity - total), &got, nullptr) || got == 0) {
            break;
        }
        total += got;
    }
    CloseHandle(file);
    bytes[total] = 0;
    *out_contents = String8FromPtrAndLength(bytes, total);

    return FILE_READ_STATUS_OK;
}

//
// Directory watch
//

#define KRAFT_WIN32_WATCH_NOTIFY_BYTES KRAFT_SIZE_KB(256)
#define KRAFT_WIN32_WATCH_NAME_BYTES KRAFT_SIZE_KB(96)

struct Win32DirectoryWatch {
    HANDLE directory;
    HANDLE stop;
    HANDLE arrived;
    Thread thread;
    u8* notify;
    u8* name;
};

static void Win32DirectoryWatchCollect(DirectoryWatch* watch, Win32DirectoryWatch* platform, DWORD bytes) {
    FILE_NOTIFY_INFORMATION* record = (FILE_NOTIFY_INFORMATION*)platform->notify;
    for (;;) {
        i32 length = WideCharToMultiByte(CP_UTF8, 0, record->FileName, (i32)(record->FileNameLength / sizeof(WCHAR)), (char*)platform->name, KRAFT_WIN32_WATCH_NAME_BYTES, nullptr, nullptr);
        if (length <= 0) {
            watch->overflowed = true;
        } else {
            for (i32 index = 0; index < length; index++) {
                if (platform->name[index] == '\\') {
                    platform->name[index] = '/';
                }
            }
            String8 relative_path = String8FromPtrAndLength(platform->name, (u64)length);
            if (!watch->ignore || !watch->ignore(relative_path)) {
                DirectoryWatchPush(watch, relative_path);
            }
        }

        if (record->NextEntryOffset == 0 || (u8*)record + record->NextEntryOffset >= platform->notify + bytes) {
            break;
        }

        record = (FILE_NOTIFY_INFORMATION*)((u8*)record + record->NextEntryOffset);
    }
}

static KRAFT_THREAD_ENTRY(Win32DirectoryWatchThread) {
    DirectoryWatch* watch = (DirectoryWatch*)user_data;
    Win32DirectoryWatch* platform = (Win32DirectoryWatch*)watch->platform;
    DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
    for (;;) {
        OVERLAPPED overlapped = {};
        overlapped.hEvent = platform->arrived;
        bool started = ReadDirectoryChangesW(platform->directory, platform->notify, KRAFT_WIN32_WATCH_NOTIFY_BYTES, watch->recursive, filter, nullptr, &overlapped, nullptr);

        DWORD bytes = 0;
        bool read = false;
        DWORD error = 0;
        if (started) {
            HANDLE handles[2] = {platform->stop, platform->arrived};
            DWORD woke = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (woke == WAIT_OBJECT_0) {
                CancelIoEx(platform->directory, &overlapped);
                GetOverlappedResult(platform->directory, &overlapped, &bytes, TRUE);
                return;
            }
            read = GetOverlappedResult(platform->directory, &overlapped, &bytes, FALSE);
            error = read ? 0 : GetLastError();
        } else {
            error = GetLastError();
        }

        MutexLock(&watch->lock);
        bool was_pending = DirectoryWatchHasPending(watch);
        u64 used_before = watch->queue_used;
        bool overflowed_before = watch->overflowed;
        bool give_up = false;
        if (read && bytes > 0) {
            Win32DirectoryWatchCollect(watch, platform, bytes);
        } else if (read || error == ERROR_NOTIFY_ENUM_DIR) {
            // The system's buffer overflowed
            watch->overflowed = true;
        } else {
            watch->lost = true;
            give_up = true;
        }

        bool arrived = give_up || watch->queue_used != used_before || watch->overflowed != overflowed_before;
        if (arrived) {
            DirectoryWatchStamp(watch, was_pending);
        }
        MutexUnlock(&watch->lock);

        if (arrived && watch->wake) {
            watch->wake();
        }

        if (give_up) {
            return;
        }
    }
}

KRAFT_API DirectoryWatch* DirectoryWatchStart(String8 path, bool recursive, DirectoryWatchIgnore* ignore, DirectoryWatchWake* wake) {
    TempArena scratch = ScratchBegin(0, 0);
    HANDLE directory = CreateFileW(
        Win32WidePath(scratch.arena, path),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    ScratchEnd(scratch);
    if (directory == INVALID_HANDLE_VALUE) {
        KWARN("[DirectoryWatch]: cannot watch %S (error %lu)", path, (unsigned long)GetLastError());
        return nullptr;
    }

    DirectoryWatch* watch = DirectoryWatchCreate(recursive, ignore, wake);
    Win32DirectoryWatch* platform = ArenaPush(watch->arena, Win32DirectoryWatch);
    platform->directory = directory;
    platform->notify = (u8*)ArenaPushArrayAligned(watch->arena, DWORD, KRAFT_WIN32_WATCH_NOTIFY_BYTES / sizeof(DWORD));
    platform->name = ArenaPushArray(watch->arena, u8, KRAFT_WIN32_WATCH_NAME_BYTES);
    platform->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    platform->arrived = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    watch->platform = platform;
    platform->thread = ThreadLaunch(Win32DirectoryWatchThread, watch, String8Raw("directory watch"));
    if (!platform->thread.handle) {
        CloseHandle(platform->stop);
        CloseHandle(platform->arrived);
        CloseHandle(directory);
        DirectoryWatchDestroy(watch);
        return nullptr;
    }

    return watch;
}

KRAFT_API void DirectoryWatchStop(DirectoryWatch* watch) {
    if (!watch) {
        return;
    }

    Win32DirectoryWatch* platform = (Win32DirectoryWatch*)watch->platform;
    SetEvent(platform->stop);
    ThreadJoin(platform->thread);
    CloseHandle(platform->stop);
    CloseHandle(platform->arrived);
    CloseHandle(platform->directory);
    DirectoryWatchDestroy(watch);
}

KRAFT_API bool RemoveEmptyDirectory(String8 path) {
    if (RemoveDirectoryA((char*)path.ptr)) {
        return true;
    }

    DWORD error = GetLastError();

    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

KRAFT_API Directory ReadDirEntries(ArenaAllocator* arena, String8 path) {
    TempArena scratch = ScratchBegin(&arena, 1);
    Directory result = {};
    String8 pattern = path;
    if (!StringEndsWith(pattern, String8Raw("/*")) && !StringEndsWith(pattern, String8Raw("\\*"))) {
        pattern = StringCat(scratch.arena, pattern, String8Raw("/*"));
    }

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile((char*)pattern.ptr, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        ScratchEnd(scratch);
        return result;
    }

    u32 entry_count = 0;
    do {
        if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0)
            entry_count++;
    } while (FindNextFile(find_handle, &find_data) != 0);
    FindClose(find_handle);

    result.entries = ArenaPushArray(arena, FileSystemEntry, entry_count);
    find_handle = FindFirstFile((char*)pattern.ptr, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        ScratchEnd(scratch);
        return result;
    }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;
        if (result.entry_count >= entry_count)
            break;
        FileSystemEntry* entry = &result.entries[result.entry_count++];
        entry->name = ArenaPushString8Copy(arena, String8FromCString(find_data.cFileName));
        entry->is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        LARGE_INTEGER file_size;
        file_size.LowPart = find_data.nFileSizeLow;
        file_size.HighPart = find_data.nFileSizeHigh;
        entry->file_size = entry->is_directory ? 0 : (u64)file_size.QuadPart;

        // The enumeration already has the timestamp, so a caller that wants it does not have to
        // stat every file again
        // FILETIME counts 100ns intervals from 1601.
        ULARGE_INTEGER write_time;
        write_time.LowPart = find_data.ftLastWriteTime.dwLowDateTime;
        write_time.HighPart = find_data.ftLastWriteTime.dwHighDateTime;
        entry->modified_time = write_time.QuadPart > 116444736000000000ull ? (write_time.QuadPart - 116444736000000000ull) / 10000000ull : 0;
    } while (FindNextFile(find_handle, &find_data) != 0);
    FindClose(find_handle);
    ScratchEnd(scratch);

    return result;
}

KRAFT_API u32 GetFileCount(String8 path) {
    u32 result = 0;
    TempArena scratch = ScratchBegin(0, 0);
    String8 dir = path;
    if (!StringEndsWith(dir, String8Raw("/*")) && !StringEndsWith(dir, String8Raw("\\*"))) {
        dir = StringCat(scratch.arena, dir, String8Raw("/*"));
    }

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile((char*)dir.ptr, &find_data);

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            result++;
        }
    } while (FindNextFile(find_handle, &find_data) != 0);

    FindClose(find_handle);
    ScratchEnd(scratch);

    return result;
}

KRAFT_API Directory ReadDir(ArenaAllocator* arena, String8 path) {
    TempArena scratch = ScratchBegin(&arena, 1);
    Directory result = {};
    String8 dir = path;
    if (!StringEndsWith(dir, String8Raw("/*")) && !StringEndsWith(dir, String8Raw("\\*"))) {
        dir = StringCat(scratch.arena, dir, String8Raw("/*"));
    }

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile((char*)dir.ptr, &find_data);
    LARGE_INTEGER file_size;

    if (INVALID_HANDLE_VALUE == find_handle) {
        String8 error_message = ArenaPushString8Empty(scratch.arena, u64(1024));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)error_message.ptr, error_message.count, NULL);

        KERROR("[Win32]: FindFirstFile failed with error %S", error_message);

        ScratchEnd(scratch);
        return result;
    }

    result.entry_count = GetFileCount(path);
    result.entries = ArenaPushArray(arena, FileSystemEntry, result.entry_count);
    u32 i = 0;

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            file_size.LowPart = find_data.nFileSizeLow;
            file_size.HighPart = find_data.nFileSizeHigh;
            // KDEBUG("  %s   %ld bytes", find_data.cFileName, file_size.QuadPart);

            result.entries[i++] = FileSystemEntry{.name = ArenaPushString8Copy(arena, String8FromCString(find_data.cFileName)), .file_size = (u64)file_size.QuadPart};
        }
    } while ((FindNextFile(find_handle, &find_data) != 0) && i < result.entry_count);

    FindClose(find_handle);
    ScratchEnd(scratch);

    return result;
}

} // namespace kraft::fs