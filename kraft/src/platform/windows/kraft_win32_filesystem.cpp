#include <platform/kraft_filesystem.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <containers/kraft_array.h>
#include <core/kraft_asserts.h>
#include <core/kraft_string.h>
#include <core/kraft_log.h>

namespace kraft::fs {

FileMMapHandle MMap(const String& Path)
{
    HANDLE File = CreateFileA(*Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (File == INVALID_HANDLE_VALUE)
    {
        return { 0 };
    }

    HANDLE FileMapping = CreateFileMapping(File, NULL, PAGE_READONLY, 0, 0, NULL);
    KASSERT(FileMapping != INVALID_HANDLE_VALUE);

    LPVOID FileMapView = MapViewOfFile(FileMapping, FILE_MAP_READ, 0, 0, 0);
    KASSERT(FileMapView != nullptr);

    FileMMapHandle Out;
    Out.Handle = FileMapView;
    Out.Size = (uint64)(::GetFileSize(File, NULL));

    return Out;
}

KRAFT_API bool FileExists(String8 path)
{
    DWORD attributes = GetFileAttributes((char*)path.ptr);

    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

KRAFT_API u32 GetFileCount(String8 path)
{
    u32       result = 0;
    TempArena scratch = ScratchBegin(0, 0);
    String8   dir = path;
    if (!StringEndsWith(dir, String8Raw("/*")) && !StringEndsWith(dir, String8Raw("\\*")))
    {
        dir = StringCat(scratch.arena, dir, String8Raw("/*"));
    }

    WIN32_FIND_DATA find_data;
    HANDLE          find_handle = FindFirstFile((char*)dir.ptr, &find_data);

    do
    {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            result++;
        }
    } while (FindNextFile(find_handle, &find_data) != 0);

    FindClose(find_handle);
    ScratchEnd(scratch);

    return result;
}

KRAFT_API Directory ReadDir(ArenaAllocator* arena, String8 path)
{
    TempArena scratch = ScratchBegin(0, 0);
    Directory result = {};
    String8   dir = path;
    if (!StringEndsWith(dir, String8Raw("/*")) && !StringEndsWith(dir, String8Raw("\\*")))
    {
        dir = StringCat(scratch.arena, dir, String8Raw("/*"));
    }

    WIN32_FIND_DATA find_data;
    HANDLE          find_handle = FindFirstFile((char*)dir.ptr, &find_data);
    LARGE_INTEGER   file_size;

    if (INVALID_HANDLE_VALUE == find_handle)
    {
        String8 error_message = ArenaPushString8Empty(scratch.arena, u64(1024));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)error_message.ptr, error_message.count, NULL);

        KERROR("[Win32]: FindFirstFile failed with error %S", error_message);

        ScratchEnd(scratch);
        return result;
    }

    result.entry_count = GetFileCount(path);
    result.entries = ArenaPushArray(arena, FileSystemEntry, result.entry_count);
    u32 i = 0;

    do
    {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            file_size.LowPart = find_data.nFileSizeLow;
            file_size.HighPart = find_data.nFileSizeHigh;
            // KDEBUG("  %s   %ld bytes", find_data.cFileName, file_size.QuadPart);

            result.entries[i++] = FileSystemEntry{ .name = ArenaPushString8Copy(arena, String8FromCString(find_data.cFileName)), .file_size = (u64)file_size.QuadPart };
        }
    } while ((FindNextFile(find_handle, &find_data) != 0) && i < result.entry_count);

    FindClose(find_handle);
    ScratchEnd(scratch);

    return result;
}

} // namespace kraft::fs