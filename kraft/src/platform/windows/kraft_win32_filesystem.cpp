#include <platform/kraft_filesystem.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <core/kraft_core_includes.h>

namespace kraft::fs {

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

    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
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