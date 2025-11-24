#include "kraft_filesystem.h"

#include <cerrno>

#include <core/kraft_allocators.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>

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
    WString WideStringPath = MultiByteStringToWideCharString(path.ptr);
    WString WideStringMode = MultiByteStringToWideCharString(mode_string);
    result->Handle = _wfopen(*WideStringPath, *WideStringMode);
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

// outBuffer, if null is allocated
// outBuffer must be freed by the caller
bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, u64* bytesRead)
{
    if (!handle->Handle)
        return false;

    fseek(handle->Handle, 0, SEEK_END);
    u64 size = ftell(handle->Handle);
    rewind(handle->Handle);

    if (!*outBuffer)
        *outBuffer = (uint8*)Malloc(size, MEMORY_TAG_FILE_BUF);

    size_t read = fread(*outBuffer, 1, size, handle->Handle);

    KASSERT(read == size);
    // Not all bytes were read
    if (read != size)
    {
        return false;
    }

    if (bytesRead)
    {
        *bytesRead = read;
    }

    return true;
}

KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, FileHandle* handle)
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

KRAFT_API buffer ReadAllBytes(ArenaAllocator* arena, String8 path)
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

bool WriteFile(FileHandle* Handle, const uint8* Buffer, u64 Size)
{
    if (!Handle->Handle)
        return false;

    fwrite(Buffer, sizeof(uint8), Size, Handle->Handle);

    return true;
}

bool WriteFile(FileHandle* Handle, const Buffer& DataBuffer)
{
    if (!Handle->Handle)
        return false;

    fwrite(*DataBuffer, sizeof(Buffer::ValueType), DataBuffer.Length, Handle->Handle);

    return true;
}

void CleanPath(const char* path, char* out)
{
    u64 Length = StringLength(path);
    for (int i = 0; i < Length; i++)
    {
        if (path[i] == '\\')
        {
            out[i] = '/';
        }
        else
        {
            out[i] = path[i];
        }
    }
}

String8 CleanPath(ArenaAllocator* arena, String8 path)
{
    String8 result = {};
    result.ptr = ArenaPushArray(arena, u8, path.count);
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

    return result;
}

void Dirname(const char* Path, char* Out)
{
    u64 Length = StringLength(Path);
    Dirname(Path, Length, Out);
}

void Dirname(const char* Path, u64 PathLength, char* Out)
{
    for (int i = (int)PathLength - 1; i >= 0; i--)
    {
        if (Path[i] == '/' || Path[i] == '\\')
        {
            StringNCopy(Out, Path, i);
            Out[i] = 0;
            return;
        }
    }

    // There is no path separator, just copy the original string back
    StringNCopy(Out, Path, PathLength);
    Out[PathLength] = 0;
}

String8 Dirname(ArenaAllocator* arena, String8 path)
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

    MemCpy(result.ptr, path.ptr, i);
    result.count = i;

    return result;
}

char* Dirname(ArenaAllocator* Arena, const String& Path)
{
    return Dirname(Arena, Path.Data(), Path.GetLengthInBytes());
}

char* Dirname(ArenaAllocator* Arena, const char* Path)
{
    return Dirname(Arena, Path, StringLength(Path));
}

KRAFT_API char* Dirname(ArenaAllocator* arena, const char* path, u64 path_length)
{
    char* out = ArenaPushArray(arena, char, path_length + 1);
    Dirname(path, path_length, out);

    return out;
}

void Basename(const char* path, char* out)
{
    u64 Length = StringLength(path);
    for (int i = (int)Length - 1; i >= 0; i--)
    {
        if (path[i] == '/' || path[i] == '\\')
        {
            StringNCopy(out, path + i + 1, Length - i + 1);
            return;
        }
    }
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
} // namespace filesystem

} // namespace kraft