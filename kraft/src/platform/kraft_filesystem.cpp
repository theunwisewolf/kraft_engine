#include "kraft_filesystem.h"

#include <cerrno>

#include <containers/kraft_buffer.h>
#include <core/kraft_allocators.h>
#include <core/kraft_asserts.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string.h>

#include <platform/kraft_filesystem_types.h>

namespace kraft {

namespace filesystem {

bool OpenFile(const String& Path, int Mode, bool Binary, FileHandle* Out)
{
    return OpenFile(Path.Data(), Mode, Binary, Out);
}

bool OpenFile(const char* Path, int Mode, bool binary, FileHandle* out)
{
    out->Handle = {};

    const char* ModeString;
    if (Mode & FILE_OPEN_MODE_APPEND)
    {
        ModeString = binary ? "a+b" : "a+";
    }
    else if ((Mode & FILE_OPEN_MODE_READ) && (Mode & FILE_OPEN_MODE_WRITE))
    {
        ModeString = binary ? "w+b" : "w+";
    }
    else if ((Mode & FILE_OPEN_MODE_READ) && (Mode & FILE_OPEN_MODE_WRITE) == 0)
    {
        ModeString = binary ? "r+b" : "r";
    }
    else if ((Mode & FILE_OPEN_MODE_READ) == 0 && (Mode & FILE_OPEN_MODE_WRITE))
    {
        ModeString = binary ? "wb" : "w";
    }
    else
    {
        KERROR("[FileSystem::OpenFile] Invalid mode %d", Mode);
        return false;
    }

#if UNICODE && defined(KRAFT_PLATFORM_WINDOWS)
    WString WideStringPath = MultiByteStringToWideCharString(Path);
    WString WideStringMode = MultiByteStringToWideCharString(ModeString);
    out->Handle = _wfopen(*WideStringPath, *WideStringMode);
#else
    out->Handle = fopen(Path, ModeString);
#endif
    if (!out->Handle)
    {
        KERROR("[FileSystem::OpenFile]: Failed to open %s with error %s", Path, StrError(errno));
        return false;
    }

    return true;
}

uint64 GetFileSize(FileHandle* handle)
{
    KASSERT(handle->Handle);

    fseek(handle->Handle, 0, SEEK_END);
    uint64 size = ftell(handle->Handle);
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

bool FileExists(const char* path)
{
    FileHandle temp;
    if (OpenFile(path, FILE_OPEN_MODE_READ, true, &temp))
    {
        CloseFile(&temp);
        return true;
    }

    return false;
}

bool FileExists(const String& Path)
{
    return FileExists(Path.Data());
}

// outBuffer, if null is allocated
// outBuffer must be freed by the caller
bool ReadAllBytes(FileHandle* handle, uint8** outBuffer, uint64* bytesRead)
{
    if (!handle->Handle)
        return false;

    fseek(handle->Handle, 0, SEEK_END);
    uint64 size = ftell(handle->Handle);
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

bool ReadAllBytes(const char* path, uint8** outBuffer, uint64* bytesRead)
{
    FileHandle handle;
    if (!OpenFile(path, FILE_OPEN_MODE_READ, true, &handle))
    {
        return false;
    }

    bool retval = ReadAllBytes(&handle, outBuffer, bytesRead);
    CloseFile(&handle);

    return retval;
}

bool WriteFile(FileHandle* Handle, const uint8* Buffer, uint64 Size)
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
    uint64 Length = StringLength(path);
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

String CleanPath(const String& Path)
{
    String Output(Path.Length, 0);
    for (int i = 0; i < Path.Length; i++)
    {
        if (Path[i] == '\\')
        {
            Output[i] = '/';
        }
        else
        {
            Output[i] = Path[i];
        }
    }

    return Output;
}

void Dirname(const char* Path, char* Out)
{
    uint64 Length = StringLength(Path);
    Dirname(Path, Length, Out);
}

void Dirname(const char* Path, uint64 PathLength, char* Out)
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

String Dirname(const String& Path)
{
    for (int i = (int)Path.Length - 1; i >= 0; i--)
    {
        if (Path[i] == '/' || Path[i] == '\\')
        {
            return String(Path, 0, i);
        }
    }

    return Path;
}

char* Dirname(ArenaAllocator* Arena, const String& Path)
{
    return Dirname(Arena, Path.Data(), Path.GetLengthInBytes());
}

char* Dirname(ArenaAllocator* Arena, const char* Path)
{
    return Dirname(Arena, Path, StringLength(Path));
}

KRAFT_API char* Dirname(ArenaAllocator* Arena, const char* Path, uint64 PathLength)
{
    char* Out = (char*)ArenaPush(Arena, PathLength + 1);
    Dirname(Path, PathLength, Out);

    return Out;
}

void Basename(const char* path, char* out)
{
    uint64 Length = StringLength(path);
    for (int i = (int)Length - 1; i >= 0; i--)
    {
        if (path[i] == '/' || path[i] == '\\')
        {
            StringNCopy(out, path + i + 1, Length - i + 1);
            return;
        }
    }
}

String Basename(const String& Path)
{
    for (int i = (int)Path.Length - 1; i >= 0; i--)
    {
        if (Path[i] == '/' || Path[i] == '\\')
        {
            return String(Path, i + 1, Path.Length - i + 1);
        }
    }

    return Path;
}

char* PathJoin(ArenaAllocator* Arena, const char* A, const char* B)
{
    uint64 StringALength = StringLength(A);
    uint64 StringBLength = StringLength(B);
    uint64 FinalPathSize = StringALength + StringBLength + sizeof(KRAFT_PATH_SEPARATOR) + 1;
    char*  OutBuffer = (char*)ArenaPush(Arena, FinalPathSize);
    char*  Ptr = OutBuffer;

    StringNCopy(Ptr, A, StringALength);
    Ptr += StringALength;

    *Ptr = KRAFT_PATH_SEPARATOR;
    Ptr += 1;

    StringNCopy(Ptr, B, StringBLength);
    Ptr += StringBLength;
    
    *Ptr = 0;

    KASSERT((Ptr - OutBuffer + 1) == FinalPathSize);

    return OutBuffer;
}
}

}